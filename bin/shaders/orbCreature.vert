#version 460 core

#include "orb/orbData.glsl"
#include "ubos/camera.glsl"

layout(location = X) in vec3 a_cubePos;

flat out int v_instanceID;

void main()
{
    OrbData orb    = orbs[gl_InstanceID];
    vec3 centre    = orb.centreAndSpeciesIdx.xyz;
    float radius   = orb.forwardAndRadius.w;

    vec3 worldPos  = a_cubePos * radius * 1.05 + centre;

    v_instanceID   = gl_InstanceID;
    gl_Position    = u_viewProj * vec4(worldPos, 1.0);
}