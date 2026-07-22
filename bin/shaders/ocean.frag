#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"
#include "ubos/atmosphere.glsl"
#include "common/celestial.glsl"
#include "common/shadows.glsl"
#include "common/sky.glsl"
#include "common/optics.glsl"
#include "common/fog.glsl"
#include "common/gBuffer.glsl"

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

layout(location = 4) uniform vec3  u_nightAmbientFloor;

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

    // 2. Thickness & Shoreline Fade
    float terrainLinear   = linearizeDepth(terrainDepth, u_nearPlane, u_farPlane);
    float waterLinear     = linearizeDepth(waterDepth, u_nearPlane, u_farPlane);
    float depthDifference = terrainLinear - waterLinear;
    float shoreFade       = clamp(depthDifference * 0.5, 0.0, 1.0);

    // 3. Day/Night cycle scalars matching deferred pass
    vec3 sunDirNorm        = normalize(u_sunDir);
    float sunElevation     = sunElevationDegrees(sunDirNorm);

    vec3 norm        = normalize(Normal);
    vec3 viewDir     = normalize(u_cameraPos - FragPos);

    DayNightFactors factors = computeDayNightFactors(sunElevation);
    float dayFactor         = factors.day;
    float nightFactor       = factors.night;
    float twilightFactor    = factors.twilight;
    float dayLightingFactor = dayLightingFactor(sunElevation);

    // 4. Base Water Body & Direct Ambient/Diffuse Lighting
    vec3 waterAlbedo  = vec3(0.01, 0.18, 0.36); 
    vec3 dayFloor     = vec3(0.04, 0.06, 0.09);
    vec3 nightFloor   = u_nightAmbientFloor; 
    vec3 ambientFloor = mix(nightFloor, dayFloor, dayLightingFactor);
    vec3 skyAmbient   = u_sunColor.rgb * 0.15 * dayLightingFactor + ambientFloor;
    vec3 ambientLight = skyAmbient * waterAlbedo;

    float sunLambert         = max(dot(norm, sunDirNorm), 0.0);
    int cascade              = selectCascade(FragPos);
    float shadowContribution = computeShadow(FragPos, norm, sunDirNorm, cascade);
    vec3 diffuseLight        = u_sunColor.rgb * sunLambert * waterAlbedo * shadowContribution * dayLightingFactor;

    // 5. Procedural Sky Reflection (Directly incorporating sky shader logic)
    vec3 reflectDir = reflect(-viewDir, norm);
    
    // Evaluate sky color along reflected ray
    vec3 reflectedSky = evaluateSkyColor(reflectDir);

    // Apply shadow map to reflected sun/bright spots so land/mountains block reflections
    reflectedSky *= mix(0.3, 1.0, shadowContribution);

    // 6. Fresnel Mix (Water-to-Air reflection curve)
    float F0      = 0.02; // Base reflectivity of water at normal incidence
    float fresnel = schlickFresnel(F0, dot(norm, viewDir));

    // Smoothly blend water body diffuse with procedural reflected sky
    vec3 finalColor = mix(ambientLight + diffuseLight, reflectedSky, fresnel);

    // 8. Exponential Height Fog Pass
    vec3 rayDir               = normalize(FragPos - u_cameraPos);
    float rayLen              = length(FragPos - u_cameraPos);
    float camHeightAboveBase  = u_cameraPos.y - u_fogBaseHeight;
    float fogAmount = computeHeightFogAmount(
        rayDir, 
        rayLen, 
        camHeightAboveBase, 
        u_fogDensity, 
        u_fogHeightFalloff
    );

    vec3 fogColor   = mix(u_fogColorNight, u_fogColorDay, dayLightingFactor);
    finalColor      = mix(finalColor, fogColor, fogAmount);

    FragColor = vec4(finalColor, 0.85 * shoreFade);
}