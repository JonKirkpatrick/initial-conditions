#version 460 core

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

struct SpeciesData {
    vec4 irisColourAndRadius;               // xyz = irisColour,    w = irisRadius
    vec4 scleraColour;                      // xyz = scleraColour,  w = spare
    vec4 tapetumColourAndPresence;          // xyz = tepetumColour, w = presence (0 or 1)
};

layout(std430, binding = 1) readonly buffer SpeciesBuffer {
    SpeciesData species[];
};

layout(location = 0) in vec3 a_cubePos;

uniform mat4 u_viewProj;

flat out int v_instanceID;

void main()
{
    OrbData orb    = orbs[gl_InstanceID];
    vec3 centre    = orb.centreAndSpeciesIdx.xyz;
    float radius   = orb.forwardAndRadius.w;

    // Scale the unit cube to the orb's radius and translate to its centre
    vec3 worldPos  = a_cubePos * radius * 1.05 + centre;

    v_instanceID   = gl_InstanceID;
    gl_Position    = u_viewProj * vec4(worldPos, 1.0);
}