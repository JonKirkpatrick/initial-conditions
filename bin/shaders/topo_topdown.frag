uniform vec2 viewportSize;
uniform float farPlane;
uniform float fovY;
uniform float aspectRatio;
uniform mat3 invRotationMatrix;
uniform float cameraYaw;
uniform float heightMax;
uniform float u_activeLayerEnabled[16];
uniform vec2 worldMin;
uniform vec2 worldSize;

uniform vec2 layer_center[16];
uniform float layer_radius[16];
uniform float layer_falloffWidth[16];
uniform float layer_topoHeight[16];

#include "topo_common.glsl"

vec3 packHeight24(float heightValue, float maxHeightValue) {
    float normalized = clamp(heightValue / max(maxHeightValue, 1e-6), 0.0, 1.0);
    float scaled = normalized * 16777215.0;
    float r = floor(scaled / 65536.0);
    float g = floor((scaled - r * 65536.0) / 256.0);
    float b = floor(scaled - r * 65536.0 - g * 256.0);
    return vec3(r, g, b) / 255.0;
}

void main() {
    vec2 uv = gl_FragCoord.xy / viewportSize;
    uv.y = 1.0 - uv.y;
    
    // Rotate UV around center (0.5, 0.5) by -camera yaw so the viewer rotates with heading
    vec2 uvCentered = uv - vec2(0.5);
    float cosYaw = cos(-cameraYaw);
    float sinYaw = sin(-cameraYaw);
    vec2 uvRotated = vec2(
        uvCentered.x * cosYaw - uvCentered.y * sinYaw,
        uvCentered.x * sinYaw + uvCentered.y * cosYaw
    ) + vec2(0.5);
    
    vec2 xz = worldMin + uvRotated * worldSize;

    float heightValue = heightAt(xz);
    vec3 rgb = packHeight24(heightValue, heightMax);
    gl_FragColor = vec4(rgb, 1.0);
}