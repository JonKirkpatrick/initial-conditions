// ==============================================================================
// == Atmosphere & Fog Data Uniform Buffer Block & Global Aliases ==============
// ==============================================================================
#define u_fogColorDay      u_fogColorDay_Block.xyz
#define u_fogColorNight    u_fogColorNight_Block.xyz
#define u_fogDensity       u_fogDensity_Block
#define u_fogBaseHeight    u_fogBaseHeight_Block
#define u_fogHeightFalloff u_fogHeightFalloff_Block

layout (std140, binding = 2) uniform AtmosphereData {
    vec4  u_fogColorDay_Block;       
    vec4  u_fogColorNight_Block;     
    float u_fogDensity_Block;
    float u_fogBaseHeight_Block;
    float u_fogHeightFalloff_Block;
    float atmoData_padding;
};