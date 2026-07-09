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

layout(location = 0) in vec3 a_cubePos;

uniform mat4 u_lightViewProj;

flat out int v_instanceID;
out vec3 v_worldPos;

void main()
{
    OrbData orb  = orbs[gl_InstanceID];
    vec3 centre  = orb.centreAndSpeciesIdx.xyz;
    float radius = orb.forwardAndRadius.w;

    // Scale unit cube to comfortably enclose the orb volume
    vec3 worldPos = a_cubePos * radius * 1.15 + centre;

    v_worldPos   = worldPos;
    v_instanceID = gl_InstanceID;
    gl_Position  = u_lightViewProj * vec4(worldPos, 1.0);
}