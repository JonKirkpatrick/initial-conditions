#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"
#include "common/terrain.glsl"

// ==============================================================================
// == Uniforms ==================================================================
// ==============================================================================
layout(location = 0) uniform float u_reliefExaggeration;
layout(location = 1) uniform bool  u_cursorMode;
layout(location = 2) uniform float u_hexSize;
layout(location = 3) uniform vec2  u_hoveredHex;
layout(location = 4) uniform vec3  u_gridColour;
layout(location = 5) uniform float u_seaLevel;

// Unified Terrain 2D Texture Arrays
layout(binding = 6) uniform sampler2DArray u_terrainDiffuseArray;
layout(binding = 7) uniform sampler2DArray u_terrainNormalArray;

in vec2 v_worldXZ;
in vec2 v_normalXZ;
in float v_worldY;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outIndices;
layout(location = 3) out vec4 outRetro;

// Material ID Constants (Matches JSON indexing: Grass=2, Rock=3, Shore=4)
const uint MAT_GRASS = 2u;
const uint MAT_ROCK  = 3u;
const uint MAT_SHORE = 4u;

// ==============================================================================
// == G-Buffer Structs ==========================================================
// ==============================================================================

struct GeometrySample {
    vec3  pos;              // world-space position
    vec3  normal;           // world-space, after relief exaggeration + horizon damping
    float dist;             // distance from camera
    float normHeight;       // height normalised to [0,1] against u_heightMax
};

struct MaterialSample {
    vec3  albedo;
    uint  materialID;          // Primary Material ID
    uint  secondaryMaterialID; // Secondary Material ID
    float blendFactor;         // Blend amount [0.0 = Primary, 1.0 = Secondary]
};

// ==============================================================================
// == Normal Helpers =============================================================
// ==============================================================================

vec3 computeNormal() {
    vec2 nxz = v_normalXZ;
    float ny = sqrt(max(0.0, 1.0 - dot(nxz, nxz)));
    return normalize(vec3(nxz.x, ny, nxz.y));
}

// ==============================================================================
// == Hex Grid ===================================================================
// ==============================================================================

vec2 hexAt(vec2 p) {
    float q = (2.0/3.0 * p.x) / u_hexSize;
    float r = (p.y / (u_hexSize * 1.7320508)) - q / 2.0;
    float s = -q - r;
    float rq = floor(q + 0.5);
    float rr = floor(r + 0.5);
    float rs = floor(s + 0.5);
    float dq = abs(rq - q);
    float dr = abs(rr - r);
    float ds = abs(rs - s);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds)       rr = -rq - rs;
    return vec2(rq, rr);
}

float hexGrid(vec2 p) {
    float q = (2.0/3.0 * p.x) / u_hexSize;
    float r = (p.y / (u_hexSize * 1.7320508)) - q / 2.0;
    float s = -q - r;
    float rq = floor(q + 0.5);
    float rr = floor(r + 0.5);
    float rs = floor(s + 0.5);
    float dq = abs(rq - q);
    float dr = abs(rr - r);
    float ds = abs(rs - s);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds) rr = -rq - rs;
    vec2 center;
    center.x = u_hexSize * 1.5 * rq;
    center.y = u_hexSize * 1.7320508 * (rr + rq / 2.0);
    vec2 delta = abs(p - center);
    float distToEdge = max(delta.y, delta.y * 0.5 + delta.x * 0.866025);
    float hexBoundary = u_hexSize * 0.866025;
    float distToLine = abs(distToEdge - hexBoundary);
    float lineWidth = u_hexSize * 0.0625;
    float halfWidth = u_hexSize * 0.015625;
    return smoothstep(halfWidth, halfWidth * 0.625, distToLine);
}

// ==============================================================================
// == Triplanar Samplers for Texture Arrays ======================================
// ==============================================================================

vec4 sampleTriplanarArray(sampler2DArray texArray, vec3 worldPos, vec3 normal, float scale, uint layer) {
    vec3 blend = abs(normal);
    blend = max(blend - 0.2, 0.0); // Sharpen projection transition edges
    blend /= (blend.x + blend.y + blend.z);

    float layerIdx = float(layer);
    vec4 xTex = texture(texArray, vec3(worldPos.zy * scale, layerIdx));
    vec4 yTex = texture(texArray, vec3(worldPos.xz * scale, layerIdx));
    vec4 zTex = texture(texArray, vec3(worldPos.xy * scale, layerIdx));

    return xTex * blend.x + yTex * blend.y + zTex * blend.z;
}

vec3 sampleTriplanarNormalArray(sampler2DArray normArray, vec3 worldPos, vec3 normal, float scale, uint layer) {
    vec3 blend = abs(normal);
    blend = max(blend - 0.2, 0.0);
    blend /= (blend.x + blend.y + blend.z);

    float layerIdx = float(layer);

    // Unpack normal map from [0, 1] to [-1, 1]
    vec3 tX = texture(normArray, vec3(worldPos.zy * scale, layerIdx)).rgb * 2.0 - 1.0;
    vec3 tY = texture(normArray, vec3(worldPos.xz * scale, layerIdx)).rgb * 2.0 - 1.0;
    vec3 tZ = texture(normArray, vec3(worldPos.xy * scale, layerIdx)).rgb * 2.0 - 1.0;

    // Swizzle normals according to projection plane orientation
    tX = vec3(tX.xy, tX.z * sign(normal.x));
    tY = vec3(tY.xy, tY.z * sign(normal.y));
    tZ = vec3(tZ.xy, tZ.z * sign(normal.z));

    // Combine orientations
    vec3 worldNormX = vec3(0.0, tX.y, tX.x);
    vec3 worldNormY = vec3(tY.x, 0.0, tY.y);
    vec3 worldNormZ = vec3(tZ.x, tZ.y, 0.0);

    vec3 perturbed = worldNormX * blend.x + worldNormY * blend.y + worldNormZ * blend.z;
    return normalize(normal + perturbed * 0.75);
}

// Distance-Based Dual Scale Triplanar Helper for Diffuse
vec3 sampleLayerDualScale(sampler2DArray texArray, vec3 pos, vec3 normal, uint layer, float distBlend) {
    const float microScale = 0.12;
    const float macroScale = 0.015;

    vec3 close = sampleTriplanarArray(texArray, pos, normal, microScale, layer).rgb;
    vec3 far   = sampleTriplanarArray(texArray, pos, normal, macroScale, layer).rgb;
    return mix(close, far, distBlend);
}

// Distance-Based Dual Scale Triplanar Helper for Normals
vec3 sampleNormalLayerDualScale(sampler2DArray normArray, vec3 pos, vec3 normal, uint layer, float distBlend) {
    const float microScale = 0.12;
    const float macroScale = 0.015;

    vec3 closeNorm = sampleTriplanarNormalArray(normArray, pos, normal, microScale, layer);
    vec3 farNorm   = sampleTriplanarNormalArray(normArray, pos, normal, macroScale, layer);
    
    return normalize(mix(closeNorm, farNorm, distBlend));
}

// ==============================================================================
// == Terrain Material Helper ===================================================
// ==============================================================================

vec3 calculateTerrainColour(
    GeometrySample geo, 
    inout vec3 outNormal, 
    out uint primaryMatID,
    out uint secondaryMatID,
    out float blendFactor
) {
    // --- 1. Distance & Scaled Texture Sampling ---
    float distBlend = smoothstep(15.0, 75.0, geo.dist);

    vec3 grassTexColour = sampleLayerDualScale(u_terrainDiffuseArray, geo.pos, geo.normal, MAT_GRASS, distBlend);
    vec3 rockTexColour  = sampleLayerDualScale(u_terrainDiffuseArray, geo.pos, geo.normal, MAT_ROCK,  distBlend);
    vec3 shoreTexColour = sampleLayerDualScale(u_terrainDiffuseArray, geo.pos, geo.normal, MAT_SHORE, distBlend);

    // --- 2. Slope / Cliff Blending ---
    float slopeCos    = geo.normal.y; 
    float cliffFactor = 1.0 - smoothstep(0.80, 0.90, slopeCos);

    // Blend between grass and cliff rock based on slope
    vec3 baseColour = mix(grassTexColour, rockTexColour, cliffFactor);

    // --- 3. Shoreline Proximity ---
    float heightAboveSea = geo.pos.y - u_seaLevel;
    float shoreBandWidth = 20.0; 
    float shoreFactor    = 1.0 - smoothstep(0.0, shoreBandWidth, abs(heightAboveSea));

    // Combine slope and shoreline colors
    vec3 finalTerrainColour = mix(baseColour, shoreTexColour, shoreFactor);

    // --- 4. Perturb Normal Maps (Now with Dual-Scale Sampling!) ---
    // Perturb for cliff face or shoreline wet rocks
    if (cliffFactor > 0.01 || shoreFactor > 0.01) {
        uint normLayer = (shoreFactor > cliffFactor) ? MAT_SHORE : MAT_ROCK;
        float normIntensity = max(cliffFactor, shoreFactor);

        // Fetch dual-scale perturbed normal (uses microScale close up, macroScale far away)
        vec3 perturbedNormal = sampleNormalLayerDualScale(u_terrainNormalArray, geo.pos, geo.normal, normLayer, distBlend);
        outNormal = normalize(mix(outNormal, perturbedNormal, normIntensity));
    }

    // --- 5. Determine Dual Material Indices & Blend Weight for G-Buffer SSBO Lookup ---
    // Default Base: Grass
    primaryMatID   = MAT_GRASS; 
    secondaryMatID = MAT_GRASS;
    blendFactor    = 0.0;

    // Shoreline takes precedence near sea level
    if (shoreFactor > 0.001) {
        primaryMatID   = (cliffFactor > 0.5) ? MAT_ROCK : MAT_GRASS; // Cliff or Grass under the shore
        secondaryMatID = MAT_SHORE;                                   // Shore material
        blendFactor    = clamp(shoreFactor, 0.0, 1.0);
    } 
    // Otherwise blend between Grass and Cliff Rock
    else if (cliffFactor > 0.001) {
        primaryMatID   = MAT_GRASS;
        secondaryMatID = MAT_ROCK;
        blendFactor    = clamp(cliffFactor, 0.0, 1.0);
    }

    return finalTerrainColour;
}

// ==============================================================================
// == PHASE 1 — Geometry ========================================================
// ==============================================================================

GeometrySample resolveGeometry()
{
    GeometrySample geo;

    vec2 xz = v_worldXZ;
    float h = v_worldY;
    geo.pos = vec3(xz.x, h, xz.y);
    geo.dist = length(geo.pos - u_cameraPos);

    vec3 rawNormal = computeNormal();
    vec3 exagNormal = normalize(vec3(rawNormal.x * u_reliefExaggeration,
                                     rawNormal.y,
                                     rawNormal.z * u_reliefExaggeration));

    geo.normal = exagNormal;
    geo.normHeight = clamp(h / max(u_heightMax, 1.0), 0.0, 1.0);

    return geo;
}

// ==============================================================================
// == PHASE 2 — Material ========================================================
// ==============================================================================

MaterialSample resolveMaterial(inout GeometrySample geo)
{
    MaterialSample mat;

    // Calculate dynamic slope/shore terrain color + perturb normals + fetch dual material parameters
    vec3 rawTerrainColour = calculateTerrainColour(
        geo, 
        geo.normal, 
        mat.materialID, 
        mat.secondaryMaterialID, 
        mat.blendFactor
    );

    // ================== HEX GRID ==================
    float gridFade = pow(clamp(1.0 - (geo.dist / u_farPlane), 0.0, 1.0), 2.0);
    float visibilityDist = 20.0 + u_cameraHeight * 2.0;
    float distanceFade = clamp(1.0 - (geo.dist / visibilityDist), 0.0, 1.0);
    float gridMask = hexGrid(geo.pos.xz);
    float finalGridIntensity = gridMask * gridFade * distanceFade * 0.72;
    vec3 finalColor = mix(rawTerrainColour, u_gridColour, finalGridIntensity);

    // ================== HEX HIGHLIGHT ==================
    if (u_cursorMode) {
        vec2 cell = hexAt(geo.pos.xz);
        if (cell.x == u_hoveredHex.x && cell.y == u_hoveredHex.y) {
            float insideCell = 1.0 - gridMask;
            finalColor = mix(finalColor, u_gridColour, insideCell * 0.25);
            finalColor = mix(finalColor, u_gridColour, gridMask * 0.9);
        }
    }

    mat.albedo = finalColor; 

    return mat;
}

// ==============================================================================
// == Main ======================================================================
// ==============================================================================

void main() {
    // Two-phase evaluation
    GeometrySample geo = resolveGeometry();
    MaterialSample mat = resolveMaterial(geo); 

    // Normalize IDs to [0.0, 1.0] for the RGBA8 G-Buffer texture
    float primaryMatID   = (float(mat.materialID) + 0.5) / 255.0;
    float secondaryMatID = (float(mat.secondaryMaterialID) + 0.5) / 255.0;

    outAlbedo  = vec4(mat.albedo, 1.0);
    outNormal  = vec4(geo.normal, 1.0); 
    
    // r = Primary Mat, g = Species (0 for terrain), b = Secondary Mat, a = Mix Factor
    outIndices = vec4(primaryMatID, 0.0, secondaryMatID, mat.blendFactor);
    outRetro   = vec4(0.0);
}