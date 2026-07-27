#version 460 core

#include "common/terrain.glsl"

layout(location = 0) in vec2 a_uv;

layout(location = 0) uniform mat4 u_lightViewProj;

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

    gl_Position = u_lightViewProj * vec4(worldX, h, worldZ, 1.0);
}