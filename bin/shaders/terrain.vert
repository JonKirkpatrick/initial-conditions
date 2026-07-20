#version 460 core

#include "ubos/camera.glsl"

// ==============================================================================
// == Remaining Vertex Uniforms =================================================
// ==============================================================================
layout(location = 0) in vec2 a_uv; // spans [0,1] across the FULL 5x5 visible grid

uniform sampler2DArray u_terrainHeightArray;
uniform vec2           u_terrainGridWorldOrigin; // world-space origin of subgrid tile [0][0]
uniform float          u_terrainTileWorldSize;   // meters per tile side (256 * 4m)
uniform int            u_terrainSliceValid[81];  // from getActiveSliceUniforms()

// REMOVED: uniform mat4 u_viewProj; (Handled by UBO)

out vec2  v_worldXZ;
out vec2  v_normalXZ;
out float v_worldY;

const int  kVisibleGridDim = 9;
const int  kTileResolution = 256;   // core texels/side
const int  kTexSide        = 257;   // stored texels/side (with apron)

void resolveTileSample(vec2 gridUV, out int layer, out vec2 texUV)
{
    vec2 tileF   = clamp(gridUV, 0.0, 1.0) * float(kVisibleGridDim);
    ivec2 tileXY = clamp(ivec2(floor(tileF)), ivec2(0), ivec2(kVisibleGridDim - 1));
    layer = tileXY.y * kVisibleGridDim + tileXY.x;

    vec2 localUV = fract(tileF);
    texUV = (localUV * float(kTileResolution) + 0.5) / float(kTexSide);
}

float decodeHeightVertex(vec2 gridUV)
{
    if (any(lessThan(gridUV, vec2(0.0))) || any(greaterThanEqual(gridUV, vec2(0.99999)))) 
    {
        return 0.0;
    }

    int layer; vec2 texUV;
    resolveTileSample(gridUV, layer, texUV);
    if (u_terrainSliceValid[layer] == 0) return 0.0;
    return textureLod(u_terrainHeightArray, vec3(texUV, float(layer)), 0.0).r;
}

void main() {
    float worldX = u_terrainGridWorldOrigin.x + a_uv.x * (kVisibleGridDim * u_terrainTileWorldSize);
    float worldZ = u_terrainGridWorldOrigin.y + a_uv.y * (kVisibleGridDim * u_terrainTileWorldSize);

    float h = decodeHeightVertex(a_uv);
    v_worldY = h;

    // One texel step, expressed in grid-UV space (whole 5x5 extent)
    vec2 gridTexelSize = 1.0 / vec2(kVisibleGridDim * kTileResolution);

    float hL = decodeHeightVertex(a_uv + vec2(-gridTexelSize.x, 0.0));
    float hR = decodeHeightVertex(a_uv + vec2( gridTexelSize.x, 0.0));
    float hD = decodeHeightVertex(a_uv + vec2(0.0, -gridTexelSize.y));
    float hU = decodeHeightVertex(a_uv + vec2(0.0,  gridTexelSize.y));

    vec2 worldTexelSize = vec2(u_terrainTileWorldSize / float(kTileResolution));
    vec3 n = normalize(vec3(hL - hR, 2.0 * worldTexelSize.x, hD - hU));

    v_normalXZ = vec2(n.x, n.z);
    v_worldXZ  = vec2(worldX, worldZ);

    gl_Position = u_viewProj * vec4(worldX, h, worldZ, 1.0);
}