#version 460 core

// ==============================================================================
// == Uniforms ==================================================================
// ==============================================================================

uniform vec3    u_cameraPos;
uniform float   u_cameraHeight;
uniform float   u_farPlane;
uniform float   u_heightMax;
uniform mat3    u_worldToCamMatrix;

uniform vec3    u_sunDir;
uniform vec4    u_sunColor;
uniform float   u_ambientStrength;

uniform bool    u_headlampOn;
uniform float   u_headlampIntensity;
uniform vec3    u_headlampColour;
uniform float   u_headlampRange;

uniform bool    u_cursorMode;
uniform float   u_hexSize;
uniform vec2    u_hoveredHex;
uniform vec3    u_gridColour;

uniform float   u_reliefExaggeration;

in vec2 v_worldXZ;
in vec2 v_normalXZ;
in float v_worldY;

out vec4 fragColor;

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
// == Atmospheric Adjustments ===================================================
// ==============================================================================


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
    vec3  lightDir = normalize(u_sunDir);
    float shade = clamp(dot(geo.normal, lightDir), 0.0, 1.0);

    vec3 rawTerrainColour = topoColour(exaggeratedH, shade);

    // ================== HEX GRID ==================
    float gridFade = pow(clamp(1.0 - (geo.dist / u_farPlane), 0.0, 1.0), 2.0);
    float visibilityDist = 800.0 + u_cameraHeight * 20.0;
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
// == PHASE 3 — Lighting ========================================================
// ==============================================================================

vec3 resolveLight(GeometrySample geo, MaterialSample mat)
{
    vec3 sunDirNorm = normalize(u_sunDir);
    float sunElevationDeg = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;
    float nightFactor = smoothstep(-5.0, -15.0, sunElevationDeg);
    float sunVis      = smoothstep(-4.0,   8.0, sunElevationDeg);

    float diff = max(dot(geo.normal, sunDirNorm), 0.0) * sunVis;
    vec3 ambient = u_ambientStrength * mix(1.0, 0.07, nightFactor) * mat.albedo;

    // ---- Headlamp ----
    vec3 headlampContribution = vec3(0.0);
    float headSpec = 0.0;
    if (u_headlampOn) {
        vec3 toFragment = geo.pos - u_cameraPos;
        float distToCamera = length(toFragment);

        if (distToCamera > 0.1) {
            vec3 toFragDir = normalize(toFragment);
            vec3 camForward = normalize(u_worldToCamMatrix * vec3(0.0, 0.0, -1.0));
            vec3 camRight = normalize(u_worldToCamMatrix * vec3(1.0, 0.0, 0.0));

            float spotCos = dot(camForward, toFragDir);
            float spotTight = pow(max(spotCos, 0.0), 48.0);
            float spotSpill = pow(max(spotCos, 0.0), 6.0) * 0.08;
            float spot = spotTight + spotSpill;

            if (spot > 0.001) {
                vec3 lightDir = -toFragDir;

                float headDiff = max(dot(geo.normal, lightDir), 0.0);
                float nearFade = smoothstep(0.0, 1.0, distToCamera);
                float distFalloff = pow(max(0.0, 1.0 - distToCamera / u_headlampRange), 1.6);
                headlampContribution = headDiff * spot * distFalloff * nearFade
                                    * u_headlampColour * u_headlampIntensity;

                vec3 fakeLightPos = u_cameraPos + camRight * 5.0;
                vec3 fakeLightDir = normalize(geo.pos - fakeLightPos);
                vec3 halfDir = normalize(-fakeLightDir + lightDir);
                headSpec = pow(max(dot(geo.normal, halfDir), 0.0), 32.0)
                        * spot * distFalloff * 0.3;

                float ambientLoad = clamp(u_ambientStrength * sunVis + (1.0 - nightFactor), 0.0, 1.0);
                float headlampVisibility = clamp(1.0 - ambientLoad, 0.3, 1.0);

                headlampContribution *= headlampVisibility;
                headSpec *= headlampVisibility;
            }
        }
    }

    vec3 diffuse = diff * u_sunColor.rgb * mat.albedo + headlampContribution;

    // ---- Moonlight ----
    vec3 topoLuma = vec3(dot(mat.albedo, vec3(0.299, 0.587, 0.114)));
    vec3 nightTopoTint = mix(topoLuma, mat.albedo, 0.35);
    vec3 moonTint = vec3(0.55, 0.68, 0.90);
    float moonlightFill = nightFactor * 0.08;
    ambient += moonlightFill * mix(moonTint, nightTopoTint, 0.5) * nightTopoTint;

    // ---- Sun specular ----
    vec3 viewDir = normalize(u_cameraPos - geo.pos);
    vec3 halfDir = normalize(sunDirNorm + viewDir);
    float spec = pow(max(dot(geo.normal, halfDir), 0.0), 32.0) * sunVis * 0.5;

    return ambient + diffuse + spec * u_sunColor.rgb * 0.8
         + headSpec * u_headlampColour;
}

// ==============================================================================
// == Main ======================================================================
// ==============================================================================

void main() {
    // Three-phase evaluation
    GeometrySample geo = resolveGeometry();
    MaterialSample mat = resolveMaterial(geo);
    vec3 groundColor   = resolveLight(geo, mat);

    fragColor = vec4(groundColor, 1.0);
}
