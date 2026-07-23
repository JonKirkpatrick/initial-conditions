#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"
#include "common/terrain.glsl"

// ==============================================================================
// == Remaining Loose Uniforms ==================================================
// ==============================================================================
layout(location = 0) uniform float u_reliefExaggeration;
layout(location = 1) uniform bool  u_cursorMode;
layout(location = 2) uniform float u_hexSize;
layout(location = 3) uniform vec2  u_hoveredHex;
layout(location = 4) uniform vec3  u_gridColour;
layout(location = 5) uniform float u_seaLevel;

// This is a temporary experiment while learning about textures in the terrain shader
layout(location = 6) uniform sampler2D u_rockARM;
layout(location = 7) uniform sampler2D u_rockDiff;
layout(location = 8) uniform sampler2D u_rockDisp;
layout(location = 9) uniform sampler2D u_rockNorm;

layout(location = 10) uniform sampler2D u_grassARM;
layout(location = 11) uniform sampler2D u_grassDiff;
layout(location = 12) uniform sampler2D u_grassDisp;
layout(location = 13) uniform sampler2D u_grassNorm;

in vec2 v_worldXZ;
in vec2 v_normalXZ;
in float v_worldY;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outIndices;
layout(location = 3) out vec4 outRetro;

// ==============================================================================
// == G-Buffer Structs ==========================================================
// ==============================================================================

struct GeometrySample {
    vec3  pos;              // world-space position
    vec3  normal;           // world-space, after relief exaggeration + horizon damping
    float dist;             // distance from camera (== "depth" in the orb shader)
    float normHeight;       // height normalised to [0,1] against u_heightMax (pre-exaggeration curve)
};

struct MaterialSample {
    vec3  albedo;
    uint  materialID;          // Primary Material ID (e.g., Grass = 2u)
    uint  secondaryMaterialID; // Secondary Material ID (e.g., Shore = 4u)
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
    float lineWidth = u_hexSize * 0.012;
    return smoothstep(lineWidth, 0.0, distToLine);
}

// ==============================================================================
// == Topo Colour ================================================================
// ==============================================================================

vec3 topoColour(float normHeight, float shade) {
    vec3 c0 = vec3(0.467, 0.631, 0.388);
    vec3 c1 = vec3(0.647, 0.753, 0.447);
    vec3 c2 = vec3(0.827, 0.816, 0.510);
    vec3 c3 = vec3(0.784, 0.667, 0.392);
    vec3 c4 = vec3(0.651, 0.529, 0.408);
    vec3 c5 = vec3(0.820, 0.800, 0.788);
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

// ==============================================================================
// == Triplanar Sampler =========================================================
// ==============================================================================

vec4 sampleTriplanar(sampler2D tex, vec3 worldPos, vec3 normal, float scale) {
    vec3 blend = abs(normal);
    blend = max(blend - 0.2, 0.0); // Sharpen transition zones
    blend /= (blend.x + blend.y + blend.z); // Normalize so weights sum to 1.0

    vec4 xTex = texture(tex, worldPos.zy * scale);
    vec4 yTex = texture(tex, worldPos.xz * scale);
    vec4 zTex = texture(tex, worldPos.xy * scale);

    return xTex * blend.x + yTex * blend.y + zTex * blend.z;
}

vec3 sampleTriplanarNormal(sampler2D normMap, vec3 worldPos, vec3 normal, float scale) {
    vec3 blend = abs(normal);
    blend = max(blend - 0.2, 0.0);
    blend /= (blend.x + blend.y + blend.z);

    // Unpack normal map from [0, 1] to [-1, 1]
    vec3 tX = texture(normMap, worldPos.zy * scale).rgb * 2.0 - 1.0;
    vec3 tY = texture(normMap, worldPos.xz * scale).rgb * 2.0 - 1.0;
    vec3 tZ = texture(normMap, worldPos.xy * scale).rgb * 2.0 - 1.0;

    // Swizzle normals according to projection plane orientation
    tX = vec3(tX.xy, tX.z * sign(normal.x));
    tY = vec3(tY.xy, tY.z * sign(normal.y));
    tZ = vec3(tZ.xy, tZ.z * sign(normal.z));

    // Combine orientations
    vec3 worldNormX = vec3(0.0, tX.y, tX.x);
    vec3 worldNormY = vec3(tY.x, 0.0, tY.y);
    vec3 worldNormZ = vec3(tZ.x, tZ.y, 0.0);

    vec3 perturbed = worldNormX * blend.x + worldNormY * blend.y + worldNormZ * blend.z;
    return normalize(normal + perturbed * 0.75); // 0.75 adjusts normal intensity
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
    // --- Palette Definitions ---
    vec3 shoreColour = vec3(0.38, 0.35, 0.30); // Dark, wet coastal rock/shale

    float uvScale = 0.01;

    // Calculate distance to camera/fragment
    float dist = length(geo.pos - u_cameraPos);

    // Blend factor: 0.0 near camera, 1.0 in distance (e.g. over 50 meters)
    float distBlend = smoothstep(5.0, 50.0, dist);

    // Micro scale for close-up, Macro scale for distance
    float microScale = 0.12;
    float macroScale = 0.015;

    // Sample twice and blend
    vec3 rockClose = sampleTriplanar(u_rockDiff, geo.pos, geo.normal, microScale).rgb;
    vec3 rockFar   = sampleTriplanar(u_rockDiff, geo.pos, geo.normal, macroScale).rgb;
    vec3 rockTexColour = mix(rockClose, rockFar, distBlend);

    vec3 grassClose = sampleTriplanar(u_grassDiff, geo.pos, geo.normal, microScale).rgb;
    vec3 grassFar   = sampleTriplanar(u_grassDiff, geo.pos, geo.normal, macroScale).rgb;
    vec3 grassTexColour = mix(grassClose, grassFar, distBlend);

    // --- 2. Slope / Cliff Blending ---
    float slopeCos    = geo.normal.y; 
    float cliffFactor = 1.0 - smoothstep(0.80, 0.90, slopeCos);

    // Blend between grass texture and rock texture based on slope!
    vec3 baseColour   = mix(grassTexColour, rockTexColour, cliffFactor);

    // --- 3. Perturb Normal Map on Rock Faces ---
    if (cliffFactor > 0.01) {
        vec3 rockWorldNormal = sampleTriplanarNormal(u_rockNorm, geo.pos, geo.normal, uvScale);
        outNormal = normalize(mix(outNormal, rockWorldNormal, cliffFactor));
    }

    // --- 4. Shoreline Proximity ---
    float heightAboveSea = geo.pos.y - u_seaLevel;
    float shoreBandWidth = 20.0; 
    float shoreFactor    = 1.0 - smoothstep(0.0, shoreBandWidth, abs(heightAboveSea));

    // --- 5. Determine Dual Material Indices & Blend Weight ---
    // Default Base: Grass
    primaryMatID   = 2u; 
    secondaryMatID = 2u;
    blendFactor    = 0.0;

    // Shoreline takes precedence near sea level
    if (shoreFactor > 0.001) {
        primaryMatID   = (cliffFactor > 0.5) ? 3u : 2u; // Cliff or Grass under the shore
        secondaryMatID = 4u;                            // Shore material
        blendFactor    = clamp(shoreFactor, 0.0, 1.0);
    } 
    // Otherwise blend between Grass and Cliff Rock
    else if (cliffFactor > 0.001) {
        primaryMatID   = 2u; // Grass
        secondaryMatID = 3u; // Cliff Rock
        blendFactor    = clamp(cliffFactor, 0.0, 1.0);
    }

    return mix(baseColour, shoreColour, shoreFactor);
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

    // Calculate dynamic slope/shore terrain color + perturb rock normals + fetch dual material parameters
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