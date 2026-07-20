#version 460 core

#include "ubos/camera.glsl"
#include "ubos/environment.glsl"

// ==============================================================================
// == Remaining Loose Uniforms ==================================================
// ==============================================================================
uniform float u_heightMax;
uniform float u_reliefExaggeration;

uniform bool  u_cursorMode;
uniform float u_hexSize;
uniform vec2  u_hoveredHex;
uniform vec3  u_gridColour;

in vec2 v_worldXZ;
in vec2 v_normalXZ;
in float v_worldY;

layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outIndices;
layout (location = 3) out vec4 outRetro;

// ==============================================================================
// == G-Buffer Structs ==========================================================
// ==============================================================================

struct GeometrySample {
    vec3  pos;            // world-space position
    vec3  normal;         // world-space, after relief exaggeration + horizon damping
    float dist;            // distance from camera (== "depth" in the orb shader)
    float normHeight;      // height normalised to [0,1] against u_heightMax (pre-exaggeration curve)
};

struct MaterialSample {
    vec3  albedo;          // topo colour, lit by sun elevation/shade and damped toward atmosphere
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

MaterialSample resolveMaterial(GeometrySample geo)
{
    MaterialSample mat;

    float exaggeratedH = pow(geo.normHeight, 1.0 / max(u_reliefExaggeration, 0.01));
    
    vec3 rawTerrainColour = topoColour(exaggeratedH, 1.0); 

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
    GeometrySample geo  = resolveGeometry();
    MaterialSample mat  = resolveMaterial(geo);

    outAlbedo           = vec4(mat.albedo, 1.0);
    outNormal           = vec4(geo.normal, 1.0);
    outIndices          = vec4(2.0 / 255.0, 1.0, 1.0, 1.0);
    outRetro            = vec4(0.0);
}
