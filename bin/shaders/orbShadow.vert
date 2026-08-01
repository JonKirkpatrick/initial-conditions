#version 460 core

#include "orb/orbData.glsl"

layout(location = X) in vec3 a_cubePos;

layout(location = X) uniform mat4 u_lightViewProj;

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
