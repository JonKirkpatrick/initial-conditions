#version 460 core

struct SpeciesData {
    vec4 irisColourAndRadius;               // xyz = irisColour,    w = irisRadius
    vec4 scleraColour;                      // xyz = scleraColour,  w = spare
    vec4 tapetumColourAndPresence;          // xyz = tepetumColour, w = presence (0 or 1)
};

layout(std430, binding = 1) readonly buffer SpeciesBuffer {
    SpeciesData species[];
};

struct MaterialData {
    // === Block 1: Base Color / Surface Tint ===
    vec4 albedoTint;                        // Multiplier for texture or flat color

    // === Block 2: Physically-Based Material Attributes ===
    float roughness;                        // 0.0 (smooth/mirror) to 1.0 (rough/matte)
    float metallic;                         // 0.0 (dielectric) to 1.0 (pure metal)
    float emissiveIntensity;                // Self-illumination factor
    float specularReflectance;              // Base specular level for non-metals (standard default is 0.04)

    // === Block 3: Texture Mapping Properties ===
    vec2 uvScale;                           // Tiling scales
    vec2 uvOffset;                          // Texture shifting

    // === Block 4: Pipeline Behavior & Logic ===
    uint  materialFlags;                    // Bitmask (e.g., Bit 0: Unlit, Bit 1: Two-Sided)
    float spare0;                           // Available slots for future expansion
    float spare1;
    float spare2;
};

layout(std430, binding = 2) readonly buffer MaterialBuffer {
    MaterialData material[];
};

in vec2 v_uv;
out vec4 FragColor;

// G-Buffer Uniform Samplers
uniform sampler2D u_gAlbedo;
uniform sampler2D u_gNormal;
uniform sampler2D u_gIndices;
uniform sampler2D u_gRetro;
uniform sampler2D u_gDepth;

// World Reconstruction
uniform mat4 u_invViewProj;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraForward;

// Lighting Uniforms
uniform vec3 u_sunDir;
uniform vec4 u_sunColor;
uniform float u_headlampIntensity;
uniform float u_headlampRange;
uniform float u_headlampConeCos;
uniform float u_headlampEnabled;

// Shadow Uniforms
uniform sampler2DArrayShadow u_shadowMap;
uniform mat4      u_lightViewProj[5];
uniform float     u_cascadeSplitDepths[5];
uniform float     u_lightDepthRange[5];
uniform float     u_texelWorldSize[5];
uniform bool      u_debugShowCascadeColors;

const vec3 cascadeDebugColors[5] = vec3[](
    vec3(1.0, 0.3, 0.3), 
    vec3(0.3, 1.0, 0.3), 
    vec3(0.3, 0.3, 1.0), 
    vec3(1.0, 1.0, 0.3), 
    vec3(1.0, 0.3, 1.0)
);

int selectCascade(vec3 worldPos) {
    float viewDepth = dot(worldPos - u_cameraPos, u_cameraForward);
    for (int i = 0; i < 4; ++i)
        if (viewDepth < u_cascadeSplitDepths[i]) return i;
    return 4;  // fall through to the fill cascade, not back to 3
}

vec3 reconstructWorldPos(vec2 uv, float rawDepth) {
    vec4 ndc = vec4(
        uv.x * 2.0 - 1.0,
        uv.y * 2.0 - 1.0,
        rawDepth * 2.0 - 1.0,
        1.0
    );
    
    vec4 worldPosPadded = u_invViewProj * ndc;
    
    return worldPosPadded.xyz / worldPosPadded.w;
}

float computeShadow(vec3 worldPos, vec3 normal, vec3 sunDir, int cascade) {
    float NdotL = max(dot(normal, sunDir), 0.0);

    // keep this modest -- it's the main source of peter panning
    float normalOffsetTexels = mix(1.0, 0.5, NdotL);
    vec3 offsetPos = worldPos + normal * (normalOffsetTexels * u_texelWorldSize[cascade]);

    vec4 lightClip = u_lightViewProj[cascade] * vec4(offsetPos, 1.0);
    vec3 lightNdc  = lightClip.xyz / lightClip.w;
    vec3 shadowUV  = lightNdc * 0.5 + 0.5;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
        shadowUV.y < 0.0 || shadowUV.y > 1.0 ||
        shadowUV.z < 0.0 || shadowUV.z > 1.0) {
        return 1.0;
    }

    // slope-scaled bias -- grows as the surface turns away from the light
    float tanTheta = sqrt(1.0 - NdotL * NdotL) / max(NdotL, 0.05);
    const float baseBias = 0.0003;
    const float maxBias  = 0.004; // clamp so extreme grazing doesn't over-bias
    float bias = clamp(baseBias * (1.0 + tanTheta), baseBias, maxBias);

    float texelSize = 1.0 / float(textureSize(u_shadowMap, 0).x);
    float sum = 0.0;
    int K = max(1, 3 - cascade);
    int taps = 0;
    for (int x = -K; x <= K; ++x) {
        for (int y = -K; y <= K; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            sum += texture(u_shadowMap, vec4(shadowUV.xy + offset, float(cascade), shadowUV.z - bias));
            taps++;
        }
    }
    return sum / float(taps);
}

void main()
{
    float depth = texture(u_gDepth, v_uv).r;

    if (depth >= 1.0) {
        discard; 
    }

    // Sample texture data
    vec4 albedoSample  = texture(u_gAlbedo, v_uv);
    vec3 normalSample  = texture(u_gNormal, v_uv).xyz;
    vec4 indicesSample = texture(u_gIndices, v_uv);
    vec4 retroSample   = texture(u_gRetro, v_uv);

    // Unpack surface properties
    vec3  albedo       = albedoSample.rgb;
    float eyelidMask   = albedoSample.a; 
    vec3  normal       = normalize(normalSample); 

    // Reconstruct SSBO indices (un-map from [0, 1] range back to integers)
    int materialID     = int(indicesSample.r * 255.0 + 0.5);
    int speciesIdx     = int(indicesSample.g * 255.0 + 0.5);

    // Grab global material PBR profiles
    MaterialData mat   = material[materialID];
    
    vec3 worldPos = reconstructWorldPos(v_uv, depth);
    vec3 viewDir  = normalize(u_cameraPos - worldPos);

    // Initialize color accumulator with a low ambient fallback
    // Calculate sun elevation for atmosphere blending
    float sunElevation = asin(clamp(normalize(u_sunDir).y, -1.0, 1.0)) * 180.0 / 3.14159265;

    // Replicate your sky blend factors to drive ambient ground colors
    float dayFactor      = smoothstep(-2.0, 12.0, sunElevation);
    float nightFactor    = smoothstep(-8.0, -18.0, sunElevation);
    float twilightFactor = 1.0 - dayFactor - nightFactor;
    twilightFactor       = clamp(twilightFactor, 0.0, 1.0);

    // Define ambient colors matching your sky's feel
    vec3 ambientDay      = vec3(0.08, 0.12, 0.20); // Cool daylight shadows
    vec3 ambientTwilight = vec3(0.22, 0.10, 0.15); // Deep magenta/warm horizon scattering
    vec3 ambientNight    = vec3(0.005, 0.005, 0.01); // Dark night sky fallback

    // Blend the ambient color dynamically
    vec3 dynamicAmbient = ambientDay * dayFactor + ambientTwilight * twilightFactor + ambientNight * nightFactor;

    // Initialize color accumulator with the new dynamic sky-ambient
    vec3 ambientLight  = dynamicAmbient * albedo * mat.albedoTint.rgb;
    vec3 diffuseLight  = vec3(0.0);
    vec3 specularLight = vec3(0.0);
    int cascade = selectCascade(worldPos);

    // 1. ==================== SUN DIRECTIONAL LIGHT ====================
    vec3 sunDirection  = normalize(u_sunDir);
    float sunLambert   = max(dot(normal, sunDirection), -0.5);

    float viewDepth = dot(worldPos - u_cameraPos, u_cameraForward);
    float shadowContribution;
    shadowContribution = computeShadow(worldPos, normal, sunDirection, cascade);

    // shadowContribution = mix(0.15, 1.0, shadowContribution);

    if (u_debugShowCascadeColors) {
        FragColor = vec4(cascadeDebugColors[cascade] * (0.4 + 0.6 * shadowContribution), 1.0);
        return;
    }
    diffuseLight      += u_sunColor.rgb * sunLambert * albedo * mat.albedoTint.rgb;
    diffuseLight      *= shadowContribution;

    // 2. ==================== PLAYER HEADLAMP LIGHT ====================
    float lampIntensityMask = 0.0;
    
    if (u_headlampEnabled > 0.5) {
        vec3 lightToFrag   = worldPos - u_cameraPos;
        float distToFrag   = length(lightToFrag);
        vec3 toFragDir     = normalize(lightToFrag);

        // Standard distance attenuation
        float distFalloff  = pow(max(0.0, 1.0 - distToFrag / u_headlampRange), 1.6);
        float nearFade     = smoothstep(0.0, 1.0, distToFrag);

        float spotCos      = dot(u_cameraForward, toFragDir);

        float spot         = pow(max(spotCos, 0.0), 48.0) + pow(max(spotCos, 0.0), 6.0) * 0.08;
        
        lampIntensityMask  = spot * distFalloff * nearFade * u_headlampIntensity;

        if (lampIntensityMask > 0.001) {
            float lampLambert = max(dot(normal, viewDir), 0.0);
            vec3 lampColour   = vec3(1.0, 0.95, 0.85); // Warm headlamp glow
            
            diffuseLight += lampColour * lampLambert * albedo * mat.albedoTint.rgb * lampIntensityMask;
        }
    }

    // 3. ==================== RETROREFLECTIVE SHINE ====================
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

            // Compute alignment between view axis and eye gaze orientation
            float alignment        = max(dot(viewDir, gazeDirWorld), 0.0);
            float retroReflection  = pow(alignment, 16.0) * lampIntensityMask;
            
            retroContribution      = sp.tapetumColourAndPresence.xyz * retroReflection * 15.0;
        }
    }

    // Combine everything together
    vec3 finalColor = ambientLight + diffuseLight + specularLight + retroContribution;

    FragColor = vec4(finalColor, 1.0);
}