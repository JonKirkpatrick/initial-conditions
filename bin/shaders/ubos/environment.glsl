// ==============================================================================
// == Environment Data Uniform Buffer Block & Global Aliases ====================
// ==============================================================================
#define u_sunDir   u_sunDir_Block
#define u_sunColor u_sunColor_Block

layout (std140, binding = 1) uniform EnvironmentData {
    vec4  u_sunColor_Block;
    vec3  u_sunDir_Block;
    float u_ambientStrength;
    vec3  u_moonDir;
    float u_skyExposure;
    vec2  u_windDirection;
    float u_windSpeed;
    float envdata_padding;
};