#version 460 core
#include "ubos/camera.glsl"
#include "ubos/environment.glsl"
#include "ubos/atmosphere.glsl"

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D u_gDepth;
uniform sampler2DArrayShadow u_shadowMap;

// Shadow & Cascade uniforms matching baseline structures
uniform mat4  u_lightViewProj[5];
uniform float u_cascadeSplitDepths[5];
uniform float u_texelWorldSize[5];

uniform vec3  u_nightAmbientFloor;

float linearizeDepth(float d, float nearZ, float farZ)
{
    float ndc = d * 2.0 - 1.0;
    return (2.0 * nearZ * farZ) / (farZ + nearZ - ndc * (farZ - nearZ));
}

int selectCascade(vec3 worldPos) {
    float viewDepth = dot(worldPos - u_cameraPos, u_cameraForward);
    for (int i = 0; i < 4; ++i) {
        if (viewDepth < u_cascadeSplitDepths[i]) return i;
    }
    return 4;
}

// Full matching filtering loop with normal-offset & slope-biasing
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
    // 1. Manual depth discard check
    vec2 screenUV = gl_FragCoord.xy / u_viewportSize;
    float terrainDepth = texture(u_gDepth, screenUV).r;
    float waterDepth = gl_FragCoord.z;
    if (terrainDepth < waterDepth)
    {
        discard;
    }

    // 2. Thickness & Shoreline Fade calculations
    float terrainLinear   = linearizeDepth(terrainDepth, u_nearPlane, u_farPlane);
    float waterLinear     = linearizeDepth(waterDepth, u_nearPlane, u_farPlane);
    float depthDifference = terrainLinear - waterLinear;
    float shoreFade       = clamp(depthDifference * 0.5, 0.0, 1.0);

    // 3. Day/Night cycle scalars matching deferred pass
    vec3 sunDirNorm    = normalize(u_sunDir);
    float sunElevation = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;
    float dayLightingFactor = smoothstep(-8.0, 12.0, sunElevation);

    // Base properties of our water material
    vec3 waterAlbedo = vec3(0.01, 0.18, 0.36); 

    // 4. Dynamic Ambient Light Matching
    vec3 dayFloor     = vec3(0.04, 0.06, 0.09);
    vec3 nightFloor    = u_nightAmbientFloor; 
    vec3 ambientFloor = mix(nightFloor, dayFloor, dayLightingFactor);
    vec3 skyColor     = u_sunColor.rgb * 0.15 * dayLightingFactor + ambientFloor;
    vec3 ambientLight = skyColor * waterAlbedo;

    // 5. Sun Direct Diffuse + Shadowing
    vec3 norm                = normalize(Normal);
    float sunLambert         = max(dot(norm, sunDirNorm), 0.0);
    int cascade              = selectCascade(FragPos);
    float shadowContribution = computeShadow(FragPos, norm, sunDirNorm, cascade);
    
    vec3 diffuseLight = u_sunColor.rgb * sunLambert * waterAlbedo * shadowContribution * dayLightingFactor;

    // Combine surface lighting passes
    vec3 finalColor = ambientLight + diffuseLight;

    // 6. Exponential Height Fog Pass
    vec3 rayDir              = normalize(FragPos - u_cameraPos);
    float rayLen              = length(FragPos - u_cameraPos);
    float camHeightAboveBase  = u_cameraPos.y - u_fogBaseHeight;
    float b                   = u_fogHeightFalloff;

    float verticalTerm;
    if (abs(rayDir.y) > 0.001) {
        verticalTerm = (1.0 - exp(-rayLen * rayDir.y * b)) / (rayDir.y * b);
    } else {
        verticalTerm = rayLen;
    }

    float fogAmount = u_fogDensity * exp(-camHeightAboveBase * b) * verticalTerm;
    fogAmount       = clamp(fogAmount, 0.0, 1.0);

    vec3 fogColor   = mix(u_fogColorNight, u_fogColorDay, dayLightingFactor);
    finalColor      = mix(finalColor, fogColor, fogAmount);

    FragColor = vec4(finalColor, 0.85 * shoreFade);
}