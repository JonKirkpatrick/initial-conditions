#version 460 core

#include "ubos/environment.glsl"
#include "ubos/atmosphere.glsl"
#include "ubos/camera.glsl"
#include "common/sky.glsl"

in vec2 v_ndc;
out vec4 fragColor;

void main() {
    mat3 worldToCamMatrix = mat3(u_cameraRight, u_cameraUp, -u_cameraForward);
    float f = tan(fovY * 0.5);
    vec3 rayDir = normalize(worldToCamMatrix * vec3(v_ndc.x * f * aspectRatio, v_ndc.y * f, -1.0));

    fragColor = vec4(evaluateSkyColor(rayDir), 1.0);
}