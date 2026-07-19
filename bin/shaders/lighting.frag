#version 460 core

// ==============================================================================
// == Global Macro Aliases =====================================================
// ==============================================================================
#define u_invViewProj      u_invViewProj_Block
#define u_cameraPos        u_cameraPos_Block
#define u_cameraForward    u_cameraForward_Block
#define u_sunDir           u_sunDir_Block
#define u_sunColor         u_sunColor_Block
#define u_fogColorDay      u_fogColorDay_Block.xyz
#define u_fogColorNight    u_fogColorNight_Block.xyz
#define u_fogDensity       u_fogDensity_Block
#define u_fogBaseHeight    u_fogBaseHeight_Block
#define u_fogHeightFalloff u_fogHeightFalloff_Block

// ==============================================================================
// == Uniform Buffer Blocks (Bound safely to 0, 1, and 2) =======================
// ==============================================================================
layout (std140, binding = 0) uniform CameraData {
    mat4 u_view;
    mat4 u_proj;
    mat4 u_viewProj;
    mat4 u_invViewProj_Block;
    
    vec3 u_cameraPos_Block;
    float fovY;
    
    vec3 u_cameraForward_Block;
    float aspectRatio;
    
    vec3 u_cameraRight;
    float u_cameraHeight;
    
    vec3 u_cameraUp;
    float u_farPlane;
    
    vec2 u_viewportSize;
    float u_nearPlane;
    float cameraData_padding;
};

layout (std140, binding = 1) uniform EnvironmentData {
    vec4  u_sunColor_Block;
    vec3  u_sunDir_Block;
    float u_ambientStrength;
    vec3  u_moonDir;
    float u_skyExposure;
};

layout (std140, binding = 2) uniform AtmosphereData {
    vec4  u_fogColorDay_Block;       
    vec4  u_fogColorNight_Block;     
    float u_fogDensity_Block;
    float u_fogBaseHeight_Block;
    float u_fogHeightFalloff_Block;
    float atmoData_padding;
};

// ==============================================================================
// == Shader Storage Buffer Objects (SSBOs - Shifted to 5 & 6) ==================
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

uniform sampler2D u_gAlbedo;
uniform sampler2D u_gNormal;
uniform sampler2D u_gIndices;
uniform sampler2D u_gRetro;
uniform sampler2D u_gDepth;
uniform sampler2D u_ssaoTex;

uniform vec3  u_nightAmbientFloor;
uniform float u_headlampIntensity;
uniform float u_headlampRange;
uniform float u_headlampEnabled;

uniform sampler2DArrayShadow u_shadowMap;
uniform mat4  u_lightViewProj[5];
uniform float u_cascadeSplitDepths[5];
uniform float u_texelWorldSize[5];

int selectCascade(vec3 worldPos) {
    float viewDepth = dot(worldPos - u_cameraPos, u_cameraForward);
    for (int i = 0; i < 4; ++i) {
        if (viewDepth < u_cascadeSplitDepths[i]) return i;
    }
    return 4;
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

    float tanTheta = sqrt(1.0 - NdotL * NdotL) / max(NdotL, 0.05);
    const float baseBias = 0.0003;
    const float maxBias  = 0.004; 
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

    // Unpack surface properties from G-Buffer
    vec4 albedoSample  = texture(u_gAlbedo, v_uv);
    vec3 normal        = normalize(texture(u_gNormal, v_uv).xyz);
    vec4 indicesSample = texture(u_gIndices, v_uv);
    vec4 retroSample   = texture(u_gRetro, v_uv);

    vec3 albedo        = albedoSample.rgb;
    int materialID     = int(indicesSample.r * 255.0 + 0.5);
    int speciesIdx     = int(indicesSample.g * 255.0 + 0.5);

    MaterialData mat   = material[materialID];
    
    vec3 worldPos      = reconstructWorldPos(v_uv, depth);
    vec3 viewDir       = normalize(u_cameraPos - worldPos);

    vec3 sunDirNorm = normalize(u_sunDir);
    float sunElevation = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;
    
    // Smoothly transition ambient from -8.0° (night) up to 12.0° (full day)
    float dayLightingFactor = smoothstep(-8.0, 12.0, sunElevation);

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

    vec3  rayDir            = normalize(worldPos - u_cameraPos);
    float rayLen            = length(worldPos - u_cameraPos);
    float camHeightAboveBase = u_cameraPos.y - u_fogBaseHeight;
    float b                 = u_fogHeightFalloff;

    float verticalTerm;
    if (abs(rayDir.y) > 0.001) {
        verticalTerm = (1.0 - exp(-rayLen * rayDir.y * b)) / (rayDir.y * b);
    } else {
        verticalTerm = rayLen;
    }

    float fogAmount = u_fogDensity * exp(-camHeightAboveBase * b) * verticalTerm;
    fogAmount       = clamp(fogAmount, 0.0, 1.0);

    vec3 fogColor   = mix(u_fogColorNight, u_fogColorDay, dayLightingFactor);

    vec3 finalColor = ambientLight + diffuseLight + retroContribution;
    finalColor      = mix(finalColor, fogColor, fogAmount);
    FragColor       = vec4(finalColor, 1.0);
}