uniform vec2 viewportSize;
uniform vec2 worldMin;
uniform vec2 worldSize;
uniform float heightMax;

uniform float u_activeLayerEnabled[16];
uniform vec2 layer_center[16];
uniform float layer_radius[16];
uniform float layer_falloffWidth[16];
uniform float layer_topoHeight[16];

#include "topo_common.glsl"

vec3 packHeight24(float heightValue, float maxHeightValue)
{
    float normalized = clamp(
        heightValue / max(maxHeightValue, 1e-6),
        0.0,
        1.0
    );

    // Prevent overflow at exactly 1.0
    normalized = min(normalized, 0.99999994);

    float scaled = floor(normalized * 16777215.0);

    float r = floor(scaled / 65536.0);
    scaled -= r * 65536.0;

    float g = floor(scaled / 256.0);
    float b = scaled - g * 256.0;

    return vec3(r, g, b) / 255.0;
}

void main() {
    vec2 uv = gl_FragCoord.xy / viewportSize;
    uv.y = 1.0 - uv.y;
    vec2 xz = worldMin + uv * worldSize;

    float h = heightAt(xz);
    vec3 rgb = packHeight24(h, heightMax);
    gl_FragColor = vec4(rgb, 1.0);
}
