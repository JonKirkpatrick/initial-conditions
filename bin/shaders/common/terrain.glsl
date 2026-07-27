#ifndef COMMON_TERRAIN_GLSL
#define COMMON_TERRAIN_GLSL

// Shared Uniforms (explicit locations match your codebase)
layout(location = 400) uniform float          u_heightMax;
layout(location = 401) uniform vec2           u_terrainGridWorldOrigin;
layout(location = 402) uniform float          u_terrainTileWorldSize;
layout(location = 403) uniform sampler2DArray u_terrainHeightArray;
layout(location = 404) uniform sampler2DArray u_terrainRoadArray;
layout(location = 405) uniform int            u_terrainSliceValid[81];

// Constants matching streamer dimensions
const int kVisibleGridDim = 9;
const int kTileResolution = 256; // core texels per side
const int kTexSide        = 257; // texels per side with apron

// -----------------------------------------------------------------------------
// Resolves a full 9x9 grid UV [0, 1] to a 2D array layer (0..80) and local tile UV.
// -----------------------------------------------------------------------------
void resolveTileSample(vec2 gridUV, out int layer, out vec2 texUV)
{
    vec2 tileF   = clamp(gridUV, 0.0, 1.0) * float(kVisibleGridDim);
    ivec2 tileXY = clamp(ivec2(floor(tileF)), ivec2(0), ivec2(kVisibleGridDim - 1));
    layer        = tileXY.y * kVisibleGridDim + tileXY.x;

    vec2 localUV = fract(tileF);
    texUV        = (localUV * float(kTileResolution) + 0.5) / float(kTexSide);
}

// -----------------------------------------------------------------------------
// Converts absolute World XZ positions into full-grid UV [0, 1].
// Returns true if position falls within the active visible subgrid.
// -----------------------------------------------------------------------------
bool worldXZToGridUV(vec2 worldXZ, out vec2 gridUV)
{
    float totalGridWorldSize = float(kVisibleGridDim) * u_terrainTileWorldSize;
    gridUV = (worldXZ - u_terrainGridWorldOrigin) / totalGridWorldSize;
    
    return all(greaterThanEqual(gridUV, vec2(0.0))) && all(lessThan(gridUV, vec2(1.0)));
}

// -----------------------------------------------------------------------------
// Calculates the 2D array slice and texture UV directly from World XZ coordinates.
// Returns layer index, or -1 if out-of-bounds / invalid.
// -----------------------------------------------------------------------------
int calculateTerrainSliceIndex(vec2 worldXZ, out vec2 texUV)
{
    vec2 gridUV;
    if (!worldXZToGridUV(worldXZ, gridUV))
    {
        texUV = vec2(0.0);
        return -1;
    }

    int layer;
    resolveTileSample(gridUV, layer, texUV);
    return layer;
}

#endif // COMMON_TERRAIN_GLSL