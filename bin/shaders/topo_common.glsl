// ============================================================================
// Shared topography shader functions for height and normal computation
// ============================================================================

// Terrain layer parameters (must be defined in each shader that uses these)
// uniform vec2 layer_center[16];
// uniform float layer_radius[16];
// uniform float layer_falloffWidth[16];
// uniform float layer_topoHeight[16];
// uniform float u_activeLayerEnabled[16];
uniform float u_warpScale;
uniform float u_warpStrength;

vec2 warpXZ(vec2 xz) {
    float x = xz.x * u_warpScale;
    float z = xz.y * u_warpScale;

    float warpedX = sin(z * 1.43 + 0.40) + 0.5 * cos((x + z) * 1.97 - 1.20);
    float warpedZ = cos(x * 1.67 - 0.90) + 0.5 * sin((x - z) * 1.31 + 0.70);

    return xz + vec2(warpedX, warpedZ) * u_warpStrength;
}

float maskFromD(float d, float falloff) {
    float t      = falloff;
    float inside = step(d, 0.0);
    float u      = t - abs(d - t);
    float g      = clamp(0.5 * (1.0 + u / (abs(u) - 1e-10)), 0.0, 1.0);
    float b      = g * ((cos(3.141592653589793 * d / (2.0 * t)) + 1.0) * 0.5);
    return inside + b;
}

float heightAt(vec2 xz) {
    xz = warpXZ(xz);
    float height = 0.0;

    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] < 0.5) continue;

        vec2  delta   = xz - layer_center[i];
        float dist    = length(delta);
        float radius  = layer_radius[i];
        float falloff = layer_falloffWidth[i];

        float d       = dist - radius;

        height += layer_topoHeight[i] * maskFromD(d, falloff);
    }
    return height;
}

float dMaskDd(float d, float falloff) {
    float t = falloff;
    if (d <= 0.0 || d >= 2.0 * t) return 0.0;
    float arg = 3.141592653589793 * d / (2.0 * t);
    return -0.5 * sin(arg) * (3.141592653589793 / (2.0 * t));
}

// Single function that returns height AND its XZ gradient in one evaluation
vec3 layerHeightAndGrad(in vec2 xz, int i) {
    vec2  delta   = xz - layer_center[i];
    float distSq  = dot(delta, delta);
    if (distSq < 1e-12) return vec3(0.0);

    float radius  = layer_radius[i];
    float falloff = layer_falloffWidth[i];
    float topo    = layer_topoHeight[i];

    float dist    = sqrt(distSq);
    float invDist = inversesqrt(distSq);

    float d       = dist - radius;

    float mask    = maskFromD(d, falloff);
    float maskD   = dMaskDd(d, falloff);

    vec2  dD_dXZ   = delta * invDist;
    vec2  grad     = dD_dXZ * maskD * topo;

    return vec3(topo * mask, grad.x, grad.y);
}

// Combined height + normal computation (single loop)
void heightAndNormal(in vec2 xz, out float h, out vec3 normal) {
    xz = warpXZ(xz);
    float totalH = 0.0;
    float dhdx   = 0.0;
    float dhdz   = 0.0;

    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] < 0.5) continue;
        vec3 hn = layerHeightAndGrad(xz, i);
        totalH += hn.x;
        dhdx   += hn.y;
        dhdz   += hn.z;
    }

    h = totalH;
    vec3 n = vec3(-dhdx, 1.0, -dhdz);
    float lenSq = dot(n, n);
    normal = (lenSq < 1e-12) ? vec3(0.0, 1.0, 0.0) : n * inversesqrt(lenSq);
}
