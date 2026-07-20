#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2DArray  u_terrainHeightArray;
uniform vec2            u_terrainGridWorldOrigin; // world-space origin of subgrid tile [0][0]
uniform float           u_terrainTileWorldSize;   // meters per tile side (256 * 4m)
uniform int             u_terrainSliceValid[81];  // from getActiveSliceUniforms()
uniform mat4            u_lightViewProj;

const int  kVisibleGridDim = 9;
const int  kTileResolution = 256;   // core texels/side
const int  kTexSide        = 257;   // stored texels/side (with apron)

void resolveTileSample(vec2 gridUV, out int layer, out vec2 texUV)
{
    vec2 tileF   = clamp(gridUV, 0.0, 1.0) * float(kVisibleGridDim);
    ivec2 tileXY = clamp(ivec2(floor(tileF)), ivec2(0), ivec2(kVisibleGridDim - 1));
    layer = tileXY.y * kVisibleGridDim + tileXY.x;

    vec2 localUV = fract(tileF); // 0..1 across this tile's 256 core texels
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

    gl_Position = u_lightViewProj * vec4(worldX, h, worldZ, 1.0);
}