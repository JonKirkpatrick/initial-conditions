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

uniform mat4 u_shadowMatrix; // Active cascade View-Projection

flat out int v_instanceID;
out vec3 v_proxyWorldPos;

void main()
{
    OrbData orb    = orbs[gl_InstanceID];
    vec3 centre    = orb.centreAndSpeciesIdx.xyz;
    float radius   = orb.forwardAndRadius.w;

    // Use your exact proxy bounding box expansion (radius * 1.05 + centre)
    vec3 worldPos  = a_cubePos * radius * 1.05 + centre;

    v_instanceID    = gl_InstanceID;
    v_proxyWorldPos = worldPos; // Pass this world coordinate down for our ray setup!
    
    // Project into current cascade light-space
    gl_Position    = u_shadowMatrix * vec4(worldPos, 1.0);
}