#version 460 core

#include "common/terrain.glsl"

// == Minimap Parameters =====================================================
layout(location = 0) uniform vec2  u_playerXZ;
layout(location = 1) uniform float u_worldRadius;

in vec2 v_uv; // Spans [0, 1] in circular minimap quad space
out vec4 fragColor;

// == Constants matching terrain grid layout =================================
const int  kVisibleGridDim = 9;
const int  kTileResolution = 256;   // core texels/side
const int  kTexSide        = 257;   // stored texels/side (with apron)

// == Colour palette =========================================================
vec3 topoColour(float normHeight, float shade) {
    vec3 c0 = vec3(0.467, 0.631, 0.388); // deep green       (0.00)
    vec3 c1 = vec3(0.647, 0.753, 0.447); // mid green        (0.15)
    vec3 c2 = vec3(0.827, 0.816, 0.510); // yellow-tan       (0.35)
    vec3 c3 = vec3(0.784, 0.667, 0.392); // warm ochre       (0.55)
    vec3 c4 = vec3(0.651, 0.529, 0.408); // brown            (0.72)
    vec3 c5 = vec3(0.820, 0.800, 0.788); // grey-white rock  (1.00)

    vec3 base;
    if      (normHeight < 0.15) base = mix(c0, c1, normHeight / 0.15);
    else if (normHeight < 0.35) base = mix(c1, c2, (normHeight - 0.15) / 0.20);
    else if (normHeight < 0.55) base = mix(c2, c3, (normHeight - 0.35) / 0.20);
    else if (normHeight < 0.72) base = mix(c3, c4, (normHeight - 0.55) / 0.17);
    else                        base = mix(c4, c5, (normHeight - 0.72) / 0.28);

    float ambient = 0.38;
    float diffuse = 0.62;
    float light   = ambient + diffuse * shade;

    vec3 litTint   = vec3(1.04, 1.01, 0.96);
    vec3 shadeTint = vec3(0.85, 0.88, 0.94);
    vec3 tint      = mix(shadeTint, litTint, shade);
    return clamp(base * light * tint, 0.0, 1.0);
}

// ================== RESOLVE TILE SAMPLE ==================
void resolveTileSample(vec2 gridUV, out int layer, out vec2 texUV)
{
    vec2 tileF   = clamp(gridUV, 0.0, 1.0) * float(kVisibleGridDim);
    ivec2 tileXY = clamp(ivec2(floor(tileF)), ivec2(0), ivec2(kVisibleGridDim - 1));
    layer = tileXY.y * kVisibleGridDim + tileXY.x;

    vec2 localUV = fract(tileF); // 0..1 across this tile's 256 core texels
    texUV = (localUV * float(kTileResolution) + 0.5) / float(kTexSide);
}

// ================== SAMPLE FLOAT HEIGHT ==================
float sampleHeight(vec2 xz) {
    // 1. Determine size of the entire 5x5 subgrid in world units
    float gridWorldSize = float(kVisibleGridDim) * u_terrainTileWorldSize;

    // 2. Convert absolute world-space xz coordinates to the relative 0..1 gridUV space
    vec2 gridUV = (xz - u_terrainGridWorldOrigin) / gridWorldSize;

    // 3. SHORT CIRCUIT: If the coordinate escapes the 5x5 grid completely,
    // immediately return a flat 0.0 height.
    if (any(lessThan(gridUV, vec2(0.0))) || any(greaterThan(gridUV, vec2(1.0)))) {
        return 0.0;
    }

    // 4. Proceed with normal tile resolving for anything safely inside the grid
    int layer; vec2 texUV;
    resolveTileSample(gridUV, layer, texUV);

    if (u_terrainSliceValid[layer] == 0) {
        return 0.0;
    }
    return textureLod(u_terrainHeightArray, vec3(texUV, float(layer)), 0.0).r;
}

// ================== NORMAL from tile array ==================
vec3 computeNormal(vec2 xz) {
    // Epsilon is the physical size of one heightmap texel in world units (meters)
    float eps = u_terrainTileWorldSize / float(kTileResolution);
    
    float hL = sampleHeight(xz + vec2(-eps, 0.0));
    float hR = sampleHeight(xz + vec2( eps, 0.0));
    float hD = sampleHeight(xz + vec2(0.0, -eps));
    float hU = sampleHeight(xz + vec2(0.0,  eps));

    return normalize(vec3(hL - hR, 2.0 * eps, hD - hU));
}

// == Main ===================================================================
void main() {
    vec2 circularCoords = v_uv * 2.0 - 1.0;
    float r = length(circularCoords);
    if (r > 1.0) discard;

    vec2 xz = u_playerXZ + circularCoords * vec2(1.0, -1.0) * u_worldRadius;
    
    float h = sampleHeight(xz);
    vec3  normal = computeNormal(xz);

    float normH  = clamp(h / max(u_heightMax, 1.0), 0.0, 1.0);
    vec3  light  = normalize(vec3(-1.0, 1.0, 1.0));
    float shade  = clamp(dot(normal, light), 0.0, 1.0);
    vec3  colour = topoColour(normH, shade);
    float edge   = smoothstep(1.0, 0.92, r);
    colour = mix(vec3(0.776, 0.902, 0.804), colour, edge);

    fragColor = vec4(colour, edge);
}