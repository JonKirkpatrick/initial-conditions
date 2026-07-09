#version 460 core

#include "orb/orbData.glsl"

layout(location = 0) in vec3 a_cubePos;

uniform mat4 u_viewProj;

flat out int v_instanceID;

void main()
{
    OrbData orb    = orbs[gl_InstanceID];
    vec3 centre    = orb.centreAndSpeciesIdx.xyz;
    float radius   = orb.forwardAndRadius.w;

    // Scale the unit cube to the orb's radius plus a little padding and translate to its centre
    vec3 worldPos  = a_cubePos * radius * 1.05 + centre;

    v_instanceID   = gl_InstanceID;
    gl_Position    = u_viewProj * vec4(worldPos, 1.0);
}
