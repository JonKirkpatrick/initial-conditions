#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"
#include "common/terrain.glsl"

// ==============================================================================
// == Uniforms ==================================================================
// ==============================================================================
layout(location = X) uniform float u_reliefExaggeration;
layout(location = X) uniform bool  u_cursorMode;
layout(location = X) uniform bool  u_drawHexGrid;
layout(location = X) uniform float u_hexSize;
layout(location = X) uniform vec2  u_hoveredHex;
layout(location = X) uniform vec3  u_gridColour;
layout(location = X) uniform float u_seaLevel;

// Unified Terrain 2D Texture Arrays
layout(location = X) uniform sampler2DArray u_terrainDiffuseArray;
layout(location = X) uniform sampler2DArray u_terrainNormalArray;

in vec2 v_worldXZ;
in vec2 v_normalXZ;
in float v_worldY;

layout(location = X) out vec4 outAlbedo;
layout(location = X) out vec4 outNormal;
layout(location = X) out vec4 outIndices;
layout(location = X) out vec4 outRetro;

// Material ID Constants (Paper Material)
const uint MAT_PAPER     = 7u;
const uint MAT_GRID_LINE = 1u;
const uint MAT_ROAD      = 5u; // Road material ID for G-Buffer

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
// == Road SDF Helpers ===========================================================
// ==============================================================================

// Bicubic (Catmull-Rom) sample of a single channel from a 2DArray
float sampleBicubicSDF(sampler2DArray tex, vec3 uvw) {
    vec2 texSize = vec2(textureSize(tex, 0).xy);
    vec2 invSize = 1.0 / texSize;
    vec2 uv = uvw.xy * texSize - 0.5;
    vec2 i  = floor(uv);
    vec2 f  = fract(uv);

    // Catmull-Rom weights
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    vec2 s0 = w0 + w1;
    vec2 s1 = w2 + w3;
    vec2 t0 = i - 1.0 + w1 / s0;
    vec2 t1 = i + 1.0 + w3 / s1;

    t0 *= invSize;
    t1 *= invSize;

    float layer = uvw.z;
    return
        texture(tex, vec3(t0.x, t0.y, layer)).r * s0.x * s0.y +
        texture(tex, vec3(t1.x, t0.y, layer)).r * s1.x * s0.y +
        texture(tex, vec3(t0.x, t1.y, layer)).r * s0.x * s1.y +
        texture(tex, vec3(t1.x, t1.y, layer)).r * s1.x * s1.y;
}

// Samples the Road SDF distance value in world meters at world XZ position
float sampleRoadSDF(vec2 worldXZ) {
    vec2 texUV; // Changed from vec3 sliceUV to vec2 texUV
    int sliceIdx = calculateTerrainSliceIndex(worldXZ, texUV);
    
    // Unused or invalid tile boundary returns max distance
    if (sliceIdx < 0 || u_terrainSliceValid[sliceIdx] == 0) {
        return 1000.0;
    }
    
    // Sample raw normalized SDF from texture layer
    float rawSDF = sampleBicubicSDF(u_terrainRoadArray, vec3(texUV, float(sliceIdx)));
    
    // Expand normalized [0,1] back into world distance (in meters)
    const float kMaxSDFDistanceMeters = 50.0;
    return rawSDF * kMaxSDFDistanceMeters;
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

float hexGridWithNormal(vec2 p, out vec2 outGradientDir) {
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

    vec2 center;
    center.x = u_hexSize * 1.5 * rq;
    center.y = u_hexSize * 1.7320508 * (rr + rq / 2.0);

    vec2 delta = p - center;
    vec2 absDelta = abs(delta);

    if (absDelta.y > absDelta.y * 0.5 + absDelta.x * 0.866025) {
        outGradientDir = vec2(0.0, sign(delta.y));
    } else {
        outGradientDir = normalize(vec2(sign(delta.x) * 0.866025, sign(delta.y) * 0.5));
    }

    float distToEdge = max(absDelta.y, absDelta.y * 0.5 + absDelta.x * 0.866025);
    float hexBoundary = u_hexSize * 0.866025;
    float distToLine = abs(distToEdge - hexBoundary);

    float halfWidth = u_hexSize * 0.03125;
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
    return normalize(normal + perturbed * 0.35);
}

// Smooth Bevel Normal Generator that preserves paper texture details
vec3 calculateSmoothGridNormal(vec2 gradDir, float gridMask, vec3 terrainNormal, float edgeBevelStrength) {
    vec3 edgeTilt = vec3(
        -gradDir.x * edgeBevelStrength, 
        0.0, 
        -gradDir.y * edgeBevelStrength
    );

    float edgeFactor = sin(gridMask * 3.14159265); 
    vec3 beveledTerrainNormal = normalize(terrainNormal + edgeTilt * edgeFactor);

    const float paperDetailRetention = 0.35; 
    vec3 lineTopNormal = normalize(mix(vec3(0.0, 1.0, 0.0), terrainNormal, paperDetailRetention));

    vec3 finalNormal = mix(terrainNormal, beveledTerrainNormal, edgeFactor);
    finalNormal = mix(finalNormal, lineTopNormal, smoothstep(0.5, 1.0, gridMask));

    return normalize(finalNormal);
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
    const float paperScale = 0.05;

    vec3 paperAlbedo = sampleTriplanarArray(u_terrainDiffuseArray, geo.pos, outNormal, paperScale, MAT_PAPER).rgb;
    outNormal        = sampleTriplanarNormalArray(u_terrainNormalArray, geo.pos, outNormal, paperScale, MAT_PAPER);

    primaryMatID   = MAT_PAPER; 
    secondaryMatID = MAT_PAPER;
    blendFactor    = 0.0;

    return mix(paperAlbedo, vec3(1.0), 0.75);
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

    // 1. Default base material setup (Paper)
    vec3 rawTerrainColour = calculateTerrainColour(
        geo, 
        geo.normal, 
        mat.materialID, 
        mat.secondaryMaterialID, 
        mat.blendFactor
    );

    vec3 finalColor = rawTerrainColour;

    // ================== ROAD SDF PASS ==================
    float distToRoad = sampleRoadSDF(geo.pos.xz);
    
    // Tweak road width & casing width (in meters)
    const float roadWidth      = 12.0;  // Core road half-width
    const float roadCasingWidth = 12.5;  // Outer ink line casing half-width
    const float roadFeather     = 0.35; // Anti-aliasing soft edge

    // Calculate core road and casing mask using smoothstep
    float roadCoreMask   = 1.0 - smoothstep(roadWidth - roadFeather, roadWidth + roadFeather, distToRoad);
    float roadCasingMask = 1.0 - smoothstep(roadCasingWidth - roadFeather, roadCasingWidth + roadFeather, distToRoad);

    // Ink casing line (dark outline along road edges)
    const vec3 roadCasingColour = vec3(0.15, 0.12, 0.10);
    // Warm vintage ink/pavement fill for road center
    const vec3 roadFillColour   = vec3(0.82, 0.76, 0.68);

    // Apply casing outline first, then draw road fill over center
    finalColor = mix(finalColor, roadCasingColour, roadCasingMask);
    finalColor = mix(finalColor, roadFillColour, roadCoreMask);

    // ================== MATERIAL INDICES SETTING ==================
    mat.materialID          = MAT_PAPER;     // Index 7
    mat.secondaryMaterialID = MAT_GRID_LINE; // Index 5
    mat.blendFactor         = 0.0;

    // ================== HEX GRID ==================
    if (u_drawHexGrid) {
        float gridFade = pow(clamp(1.0 - (geo.dist / u_farPlane), 0.0, 1.0), 2.0);
        float visibilityDist = 20.0 + u_cameraHeight * 2.0;
        float distanceFade = clamp(1.0 - (geo.dist / visibilityDist), 0.0, 1.0);
        float gridVisibility = gridFade * distanceFade;

        vec2 lineGradientDir;
        float gridMask = hexGridWithNormal(geo.pos.xz, lineGradientDir);
        float finalGridIntensity = gridMask * gridVisibility;

        finalColor = mix(finalColor, u_gridColour, finalGridIntensity);

        // Blend factor goes from 0.0 (Pure Paper) to 1.0 (Pure Line Material) 
        mat.blendFactor          = clamp(finalGridIntensity, 0.0, 1.0);

        // ================== SMOOTH BEVEL NORMAL ==================
        const float edgeBevelStrength = 0.375;
        if (gridVisibility > 0.01 && gridMask > 0.001) {
            vec3 smoothGridNormal = calculateSmoothGridNormal(lineGradientDir, gridMask, geo.normal, edgeBevelStrength);
            geo.normal = normalize(mix(geo.normal, smoothGridNormal, gridVisibility));
        }
        // ================== HEX HIGHLIGHT ==================
        if (u_cursorMode) {
            vec2 cell = hexAt(geo.pos.xz);
            if (cell.x == u_hoveredHex.x && cell.y == u_hoveredHex.y) {
                float insideCell = 1.0 - gridMask;
                finalColor = mix(finalColor, u_gridColour, insideCell * 0.25);
                finalColor = mix(finalColor, u_gridColour, gridMask * 0.9);
            }
        }
    }

    // If road is predominant, assign road secondary material
    if (roadCasingMask > 0.5) {
        mat.secondaryMaterialID = MAT_ROAD;
        mat.blendFactor = max(mat.blendFactor, roadCoreMask);
    }


    mat.albedo = finalColor; 

    return mat;
}

// ==============================================================================
// == Main ======================================================================
// ==============================================================================

void main() {
    GeometrySample geo = resolveGeometry();
    MaterialSample mat = resolveMaterial(geo); 

    float primaryMatID   = (float(mat.materialID) + 0.5) / 255.0;
    float secondaryMatID = (float(mat.secondaryMaterialID) + 0.5) / 255.0;

    outAlbedo  = vec4(mat.albedo, 1.0);
    outNormal  = vec4(geo.normal, 1.0); 
    
    outIndices = vec4(primaryMatID, 0.0, secondaryMatID, mat.blendFactor);
    outRetro   = vec4(0.0);
}