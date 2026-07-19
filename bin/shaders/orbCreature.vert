#version 460 core

#include "orb/orbData.glsl"

// ==============================================================================
// == Uniform Buffer Binding 0 (Camera Data) ====================================
// ==============================================================================
layout (std140, binding = 0) uniform CameraData {
    mat4 u_view;
    mat4 u_proj;
    mat4 u_viewProj;        // We'll pull this instantly from the UBO now!
    mat4 u_invViewProj;
    
    vec3 u_cameraPos;
    float u_fovY;
    
    vec3 u_cameraForward;
    float u_aspectRatio;
    
    vec3 u_cameraRight;
    float u_cameraHeight;
    
    vec3 u_cameraUp;
    float u_farPlane;
    
    vec2 u_viewportSize;
    float u_nearPlane;
};

layout(location = 0) in vec3 a_cubePos;

// REMOVED: uniform mat4 u_viewProj;

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