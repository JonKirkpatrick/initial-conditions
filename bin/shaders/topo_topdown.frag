uniform vec2 viewportSize;
uniform vec3 cameraPos;
uniform float farPlane;
uniform float fovY;
uniform float aspectRatio;
uniform mat3 invRotationMatrix;
uniform float heightMax;
uniform float u_activeLayerEnabled[16];

uniform vec2 topLeft;
uniform vec2 topRight;
uniform vec2 bottomLeft;
uniform vec2 bottomRight;

uniform vec2 layer_center[16];
uniform float layer_radius[16];
uniform float layer_falloffWidth[16];
uniform float layer_topoHeight[16];

float maskFromD(float d, float rd, float falloff) {
    float t = falloff;
    float inside = step(d, 0.0);
    float u = t - abs(d - t);
    float g = clamp(0.5 * (1.0 + u / (abs(u) - 1e-10)), 0.0, 1.0);
    float cosTerm = cos(3.141592653589793 * d / (2.0 * t));
    float b = g * ((cosTerm + 1.0) * 0.5);
    return inside + b;
}

float evaluateLayerHeightAt(in vec2 xz, int layerIdx) {
    vec2 delta = xz - layer_center[layerIdx];
    float distFromCenter = length(delta);
    float d = distFromCenter - layer_radius[layerIdx];
    float falloff = layer_falloffWidth[layerIdx];
    float mask = maskFromD(d, layer_radius[layerIdx], falloff);
    return layer_topoHeight[layerIdx] * mask;
}

float heightAt(vec2 xz) {
    float height = 0.0;
    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] > 0.5) {
            height += evaluateLayerHeightAt(xz, i);
        }
    }
    return height;
}

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
    vec2 top = mix(topLeft, topRight, uv.x);
    vec2 bottom = mix(bottomLeft, bottomRight, uv.x);
    vec2 xz = mix(top, bottom, uv.y);

    float heightValue = heightAt(xz);
    vec3 rgb = packHeight24(heightValue, heightMax);
    gl_FragColor = vec4(rgb, 1.0);
}
