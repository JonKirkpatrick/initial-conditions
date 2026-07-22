#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

layout(location = 0) uniform mat4 model;
layout(location = 1) uniform mat3 normalMatrix;
layout(location = 2) uniform float time;

vec2 rotate(vec2 v, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

vec3 addGerstnerWave(vec3 pos, vec2 dir, float steepness, float waveLength, inout vec3 tangent, inout vec3 binormal) {
    float k = 2.0 * 3.14159265 / waveLength;
    float c = sqrt(9.81 / k); // True phase speed (m/s) determined by gravity & wavelength
    vec2 d = normalize(dir);
    
    float f = k * (dot(d, pos.xz) - c * time * 0.5);
    float a = steepness / k;

    tangent += vec3(
        -d.x * d.x * (steepness * sin(f)),
        d.x * (steepness * cos(f)),
        -d.x * d.y * (steepness * sin(f))
    );

    binormal += vec3(
        -d.x * d.y * (steepness * sin(f)),
        d.y * (steepness * cos(f)),
        -d.y * d.y * (steepness * sin(f))
    );

    return vec3(
        d.x * (a * cos(f)),
        a * sin(f),
        d.y * (a * cos(f))
    );
}

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vec3 gridPos  = worldPos.xyz;

    vec2 windDir   = length(u_windDirection) > 0.001 ? normalize(u_windDirection) : vec2(1.0, 0.0);
    
    // Scale choppiness and steepness with wind speed
    // Low wind = calm long swells; High wind = steep, sharp surface perturbations
    float windFactor    = clamp(u_windSpeed / 10.0, 0.1, 2.5); 
    float swellFactor   = clamp(u_windSpeed / 15.0, 0.3, 1.8);

    vec3 displacement = vec3(0.0);
    vec3 tangent      = vec3(1.0, 0.0, 0.0);
    vec3 binormal     = vec3(0.0, 0.0, 1.0);

    // 1. Deep, Fast Ocean Swells (Long wavelength, moderate steepness)
    displacement += addGerstnerWave(gridPos, rotate(windDir, 0.1), 0.12 * swellFactor, 45.0, tangent, binormal);
    displacement += addGerstnerWave(gridPos, rotate(windDir, -0.2), 0.08 * swellFactor, 25.0, tangent, binormal);

    // 2. Medium Wind Waves (Aligned closely with wind vector)
    displacement += addGerstnerWave(gridPos, windDir, 0.15 * windFactor, 12.0, tangent, binormal);

    // 3. High-Frequency Surface Chop / Micro-Ripples (Strongly amplified by wind speed)
    displacement += addGerstnerWave(gridPos, rotate(windDir, 0.4), 0.18 * windFactor, 4.0, tangent, binormal);
    displacement += addGerstnerWave(gridPos, rotate(windDir, -0.5), 0.15 * windFactor, 1.8, tangent, binormal);

    worldPos.xyz += displacement;

    vec3 waveNormal = normalize(cross(binormal, tangent));
    Normal          = normalMatrix * waveNormal;

    FragPos   = worldPos.xyz;
    TexCoords = aTexCoords;

    gl_Position = u_proj * u_view * worldPos;
}