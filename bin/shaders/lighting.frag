#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"
#include "ubos/atmosphere.glsl"
#include "common/celestial.glsl"
#include "common/shadows.glsl"
#include "common/optics.glsl"
#include "common/fog.glsl"
#include "common/gBuffer.glsl"

// ==============================================================================
// == Shader Storage Buffer Objects =============================================
// ==============================================================================
struct SpeciesData {
    vec4 irisColourAndRadius;
    vec4 scleraColour;
    vec4 tapetumColourAndPresence;
};

layout(std430, binding = 5) readonly buffer SpeciesBuffer {
    SpeciesData species[];
};

struct MaterialData {
    vec4  albedoTint;
    float roughness;
    float metallic;
    float emissiveIntensity;
    float specularReflectance;
    vec2  uvScale;
    vec2  uvOffset;
    uint  materialFlags;
    float spare0;
    float spare1;
    float spare2;
};

layout(std430, binding = 6) readonly buffer MaterialBuffer {
    MaterialData material[];
};

// ==============================================================================
// == Input / Output Context & Samplers =========================================
// ==============================================================================
in vec2 v_uv;
out vec4 FragColor;

layout(location = 0) uniform vec3  u_nightAmbientFloor;
layout(location = 1) uniform float u_headlampIntensity;
layout(location = 2) uniform float u_headlampRange;
layout(location = 3) uniform float u_headlampEnabled;

layout(location = 4) uniform sampler2D u_ssaoTex;

void main()
{
    float depth = texture(u_gDepth, v_uv).r;
    if (depth >= 1.0) {
        discard; 
    }

    // Unpack surface properties from G-Buffer
    vec4 albedoSample  = texture(u_gAlbedo, v_uv);
    vec3 normal        = normalize(texture(u_gNormal, v_uv).xyz);
    vec4 indicesSample = texture(u_gIndices, v_uv);
    vec4 retroSample   = texture(u_gRetro, v_uv);

    vec3 albedo        = albedoSample.rgb;
    int materialID     = int(indicesSample.r * 255.0 + 0.5);
    int speciesIdx     = int(indicesSample.g * 255.0 + 0.5);

    MaterialData mat   = material[materialID];
    
    vec3 worldPos      = reconstructWorldPos(v_uv, depth, u_invViewProj);
    vec3 viewDir       = normalize(u_cameraPos - worldPos);

    vec3 sunDirNorm = normalize(u_sunDir);
    float sunElevation = sunElevationDegrees(sunDirNorm);
    
    // Smoothly transition ambient from -8.0° (night) up to 12.0° (full day)
    float dayLightingFactor = dayLightingFactor(sunElevation);

    float ssao = texture(u_ssaoTex, v_uv).r;
    vec3 dayFloor        = vec3(0.04, 0.06, 0.09);
    vec3 nightFloor       = u_nightAmbientFloor; 
    vec3 ambientFloor    = mix(nightFloor, dayFloor, dayLightingFactor);
    vec3 skyColor        = u_sunColor.rgb * 0.15 * dayLightingFactor + ambientFloor;
    vec3 ambientLight    = skyColor * albedo * mat.albedoTint.rgb * ssao;
    

    // 2. Diffuse Sun Light Calculation
    vec3 sunDirection  = normalize(u_sunDir);
    float sunLambert   = max(dot(normal, sunDirection), 0.0);
    
    int cascade                 = selectCascade(worldPos);
    float shadowContribution    = computeShadow(worldPos, normal, sunDirection, cascade);
    
    vec3 diffuseLight = u_sunColor.rgb * sunLambert * albedo * mat.albedoTint.rgb * shadowContribution * dayLightingFactor;

    // 3. Player Headlamp Light
    float lampIntensityMask = 0.0;
    if (u_headlampEnabled > 0.5) {
        vec3 lightToFrag   = worldPos - u_cameraPos;
        float distToFrag   = length(lightToFrag);
        vec3 toFragDir     = normalize(lightToFrag);

        float distFalloff  = pow(max(0.0, 1.0 - distToFrag / u_headlampRange), 1.6);
        float nearFade     = smoothstep(0.0, 1.0, distToFrag);
        float spotCos      = dot(u_cameraForward, toFragDir);
        float spot         = pow(max(spotCos, 0.0), 48.0) + pow(max(spotCos, 0.0), 6.0) * 0.08;
        
        lampIntensityMask  = spot * distFalloff * nearFade * u_headlampIntensity;

        if (lampIntensityMask > 0.001) {
            float lampLambert = max(dot(normal, viewDir), 0.0);
            vec3 lampColour   = vec3(1.0, 0.95, 0.85); 
            diffuseLight     += lampColour * lampLambert * albedo * mat.albedoTint.rgb * lampIntensityMask;
        }
    }

    // 4. Retroreflective Shine
    vec3 retroContribution = vec3(0.0);
    float pupilEligibility = retroSample.b;

    if (pupilEligibility > 0.5 && lampIntensityMask > 0.0) {
        SpeciesData sp = species[speciesIdx];
        
        if (sp.tapetumColourAndPresence.w > 0.5) {
            vec2 packedGaze = retroSample.rg * 2.0 - 1.0;
            vec3 gazeDirWorld;
            gazeDirWorld.xy = packedGaze;
            gazeDirWorld.z  = 1.0 - abs(packedGaze.x) - abs(packedGaze.y);
            if (gazeDirWorld.z < 0.0) {
                gazeDirWorld.xy = (1.0 - abs(gazeDirWorld.yx)) * vec2(packedGaze.x >= 0.0 ? 1.0 : -1.0, packedGaze.y >= 0.0 ? 1.0 : -1.0);
            }
            gazeDirWorld = normalize(gazeDirWorld);

            float alignment        = max(dot(viewDir, gazeDirWorld), 0.0);
            float retroReflection  = pow(alignment, 16.0) * lampIntensityMask;
            
            retroContribution      = sp.tapetumColourAndPresence.xyz * retroReflection * 15.0;
        }
    }

    // 5. Exponential Height Fog Pass
    vec3  rayDir             = normalize(worldPos - u_cameraPos);
    float rayLen             = length(worldPos - u_cameraPos);
    float camHeightAboveBase = u_cameraPos.y - u_fogBaseHeight;

    float fogAmount = computeHeightFogAmount(
        rayDir, 
        rayLen, 
        camHeightAboveBase, 
        u_fogDensity, 
        u_fogHeightFalloff
    );

    vec3 fogColor   = mix(u_fogColorNight, u_fogColorDay, dayLightingFactor);
    vec3 finalColor = ambientLight + diffuseLight + retroContribution;
    finalColor      = mix(finalColor, fogColor, fogAmount);

    FragColor       = vec4(finalColor, 1.0);
}