// ==============================================================================
// == Environment Data Uniform Buffer Block & Global Aliases ====================
// ==============================================================================

layout (std140, binding = 1) uniform EnvironmentData {
    vec4  u_sunColor;
    vec3  u_sunDir;
    float u_ambientStrength;
    vec3  u_moonDir;
    float u_skyExposure;
    vec2  u_windDirection;
    float u_windSpeed;
    float envdata_padding;
};