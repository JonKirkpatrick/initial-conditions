#version 460 core

#include "ubos/camera.glsl"
#include "common/terrain.glsl"

layout(location = X) in vec2 a_uv; // spans [0,1] across full 9x9 visible grid

out vec2  v_worldXZ;
out vec2  v_normalXZ;
out float v_worldY;

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
    float worldX = u_terrainGridWorldOrigin.x + a_uv.x * (float(kVisibleGridDim) * u_terrainTileWorldSize);
    float worldZ = u_terrainGridWorldOrigin.y + a_uv.y * (float(kVisibleGridDim) * u_terrainTileWorldSize);

    float h = decodeHeightVertex(a_uv);
    v_worldY = h;

    vec2 gridTexelSize = 1.0 / vec2(float(kVisibleGridDim * kTileResolution));

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