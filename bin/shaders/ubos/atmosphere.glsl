// ==============================================================================
// == Atmosphere & Fog Data Uniform Buffer Block & Global Aliases ==============
// ==============================================================================

layout (std140, binding = 2) uniform AtmosphereData {
    vec4  u_fogColorDay;       
    vec4  u_fogColorNight;     
    float u_fogDensity;
    float u_fogBaseHeight;
    float u_fogHeightFalloff;
    float atmoData_padding;
};