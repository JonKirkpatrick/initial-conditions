// ==============================================================================
// == orbData.glsl ==============================================================
// == Per-instance orb transform/pose data. Shared by every orb pass — both ====
// == vertex shaders (bounding-volume placement) and both fragment shaders =====
// == (compound-shape reconstruction). =========================================
// ==============================================================================
#ifndef ORB_DATA_GLSL
#define ORB_DATA_GLSL

struct OrbData {
    vec4 centreAndSpeciesIdx;               // xyz = centre,        w = species
    vec4 forwardAndRadius;                  // xyz = forward,       w = radius
    vec4 rightPadded;                       // xyz = right,         w = spare
    vec4 upPadded;                          // xyz = up,            w = spare
    vec4 gazeDirDilationAndEyelidClosure;   // xy  = gazeDir,       zw = Dilation and Eylid
};

layout(std430, binding = 0) readonly buffer OrbBuffer {
    OrbData orbs[];
};

#endif // ORB_DATA_GLSL
