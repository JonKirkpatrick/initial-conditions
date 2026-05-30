uniform vec2 viewportSize;
uniform float m_hexSize;
uniform sampler2D topoTex;
uniform vec3 cameraPos;
uniform float camHeight;
uniform float farPlane;
uniform float nearPlane;
uniform float fovY;
uniform float aspectRatio;
uniform mat3 invRotationMatrix;
uniform float u_activeLayerEnabled[16]; // 1.0 when layer i is enabled, 0.0 otherwise

uniform vec3 sunDir;
uniform vec4 sunColor;
uniform float ambientStrength;
uniform vec3 baseColor;
uniform vec3 gridColor;

uniform bool  headlampOn;
uniform float headlampIntensity;
uniform vec3  headlampColor;
uniform float headlampRange;
uniform float dampingMax;

uniform bool  cursorMode;
uniform vec2  hoveredHex;
// Terrain layer parameters (16 layers)
uniform vec2 layer_center[16];
uniform float layer_radius[16];
uniform float layer_falloffWidth[16];
uniform float layer_topoHeight[16];
uniform float u_heightMax;
uniform float u_reliefExaggeration;

uniform int u_shadowOrbCount;
uniform vec3 u_shadowOrbPos[64];
uniform float u_shadowOrbRadius[64];
uniform float u_shadowDarkness;   // global scalar, tune at runtime

#include "topo_common.glsl"

// ================== HEX CELL TEST ==================
vec2 hexAt(vec2 p) {
    float q = (2.0/3.0 * p.x) / m_hexSize;
    float r = (p.y / (m_hexSize * 1.7320508)) - q / 2.0;
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

// ================== TOPOGRAPHIC COLORING (from minimap) ==================
vec3 topoColour(float normHeight, float shade) {
    // Classic topo palette — 6 stops from lowland green to peak white
    vec3 c0 = vec3(0.467, 0.631, 0.388); // deep green       (0.00)
    vec3 c1 = vec3(0.647, 0.753, 0.447); // mid green        (0.15)
    vec3 c2 = vec3(0.827, 0.816, 0.510); // yellow-tan       (0.35)
    vec3 c3 = vec3(0.784, 0.667, 0.392); // warm ochre       (0.55)
    vec3 c4 = vec3(0.651, 0.529, 0.408); // brown            (0.72)
    vec3 c5 = vec3(0.820, 0.800, 0.788); // grey-white rock  (1.00)

    vec3 base;
    if      (normHeight < 0.15) base = mix(c0, c1, normHeight / 0.15);
    else if (normHeight < 0.35) base = mix(c1, c2, (normHeight - 0.15) / 0.20);
    else if (normHeight < 0.55) base = mix(c2, c3, (normHeight - 0.35) / 0.20);
    else if (normHeight < 0.72) base = mix(c3, c4, (normHeight - 0.55) / 0.17);
    else                        base = mix(c4, c5, (normHeight - 0.72) / 0.28);

    // Lighting
    float ambient = 0.38;
    float diffuse = 0.62;
    float light   = ambient + diffuse * shade;

    // Warm/cool tint
    vec3 litTint   = vec3(1.04, 1.01, 0.96);
    vec3 shadeTint = vec3(0.85, 0.88, 0.94);
    vec3 tint      = mix(shadeTint, litTint, shade);

    return clamp(base * light * tint, 0.0, 1.0);
}

// ================== MAIN TERRAIN COLOR ==================
vec3 getTerrainColor(vec3 normal, vec2 xz) {
    float h;
    vec3 dummyNormal;
    heightAndNormal(xz, h, dummyNormal);

    float normH = clamp(h / max(u_heightMax, 1.0), 0.0, 1.0);

    // Exaggerate: push mid-values toward the high end of the palette
    float exaggeratedH = pow(normH, 1.0 / max(u_reliefExaggeration, 0.01));

    vec3 lightDir = normalize(sunDir);
    float shade = clamp(dot(normal, lightDir), 0.0, 1.0);

    return topoColour(exaggeratedH, shade);  // use exaggeratedH, not normH
}

// ================== HEX GRID ==================
float hexGrid(vec2 p) {
    float q = (2.0/3.0 * p.x) / m_hexSize;
    float r = (p.y / (m_hexSize * 1.7320508)) - q / 2.0;
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
    center.x = m_hexSize * 1.5 * rq;
    center.y = m_hexSize * 1.7320508 * (rr + rq / 2.0);
   
    
    vec2 delta = abs(p - center);
    float distToEdge = max(delta.y, delta.y * 0.5 + delta.x * 0.866025);
    float hexBoundary = m_hexSize * 0.866025;
   
    float distToLine = abs(distToEdge - hexBoundary);
    
    // Stable line width: use a small fixed fraction of hex size for consistent fidelity
    float lineWidth = m_hexSize * 0.012;
    float smoothing = lineWidth;
   
    return smoothstep(smoothing, 0.0, distToLine);
}

float decodeTopoDepth(vec4 topo)
{
    return dot(topo.rgb,
               vec3(
                   1.0,
                   1.0 / 256.0,
                   1.0 / 65536.0
               ));
}

// ================== MAIN ==================
void main() {
    vec2 uv = gl_FragCoord.xy / viewportSize;

    vec4 topoC = texture(topoTex, uv);

    bool isTerrainHit = topoC.a >= 0.5;

    float dist = 0.0;
    vec3 worldPos;

    if (isTerrainHit) {
        float d = decodeTopoDepth(topoC);
        dist = nearPlane + d * (farPlane - nearPlane);
        
        vec2 screen = gl_FragCoord.xy;
        screen.y = viewportSize.y - screen.y;
        float x_ndc = (screen.x / viewportSize.x) * 2.0 - 1.0;
        float y_ndc = 1.0 - (screen.y / viewportSize.y) * 2.0;
        float f = tan(fovY * 0.5);
        vec3 rayDir = normalize(invRotationMatrix * vec3(x_ndc * f * aspectRatio, y_ndc * f, -1.0));
        worldPos = cameraPos + rayDir * dist;
    } 
    else {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // ================== LIGHTING ==================
    float _h;
    vec3 normal;
    if (isTerrainHit) {
        heightAndNormal(worldPos.xz, _h, normal);
    } else {
        normal = vec3(0.0, 1.0, 0.0);
    }
    float slopeMag = length(normal.xz);
    float steepened = slopeMag * u_reliefExaggeration;
    vec3 exagNormal = normalize(vec3(normal.x * u_reliefExaggeration,
                                    normal.y,
                                    normal.z * u_reliefExaggeration));
    normal = exagNormal;
    
    // === TOPOGRAPHIC COLORING ===
    vec3 terrainBaseColor = isTerrainHit ? 
                            getTerrainColor(normal, worldPos.xz) : 
                            baseColor;

    vec3 sunDirNorm = normalize(sunDir);
    float sunElevationDeg = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;

    float nightFactor = smoothstep(-5.0, -15.0, sunElevationDeg);
    float sunVis = smoothstep(-4.0, 8.0, sunElevationDeg);

    float orbShadow = 1.0;
    if (u_shadowOrbCount > 0) {
        vec3 sunN = normalize(sunDir);
        for (int i = 0; i < 64; i++) {
            if (i >= u_shadowOrbCount) break;
            vec3 toOrb = u_shadowOrbPos[i] - worldPos;
            float along = dot(toOrb, sunN);
            if (along <= 0.0) continue;
            vec3 perp = toOrb - along * sunN;
            float perpDist = length(perp);
            float r = u_shadowOrbRadius[i];
            float maxShadowLength = 800.0;
            float shadowReach = 1.0 - smoothstep(0.0, maxShadowLength, along);
            float shadow = (1.0 - smoothstep(r * 0.6, r * 1.3, perpDist)) * shadowReach;
            orbShadow = min(orbShadow, 1.0 - shadow * u_shadowDarkness);
        }
    }
    float diff = max(dot(normal, sunDirNorm), 0.0) * sunVis * orbShadow;

    vec3 ambient = ambientStrength * mix(1.0, 0.07, nightFactor) * terrainBaseColor;

    // === HEADLAMP ===
    vec3 headlampContribution = vec3(0.0);
    float headSpec = 0.0;

    if (headlampOn) {
        vec3 toFragment = worldPos - cameraPos;
        float distToCamera = length(toFragment);

        if (distToCamera > 0.1) {
            vec3 toFragDir = normalize(toFragment);
            vec3 camForward = normalize(invRotationMatrix * vec3(0.0, 0.0, -1.0));
            vec3 camRight = normalize(invRotationMatrix * vec3(1.0, 0.0, 0.0));
            float camPitchAmount = abs(asin(clamp(camForward.y, -1.0, 1.0)));
            float damping = 1.0 + (dampingMax - 1.0) * pow(cos(clamp(camPitchAmount, 0.0, 3.14 * 0.5)), 10.0);

            float spotCos = dot(camForward, toFragDir);
            float spotTight = pow(max(spotCos, 0.0), 48.0);
            float spotSpill = pow(max(spotCos, 0.0), 6.0) * 0.08;
            float spot = spotTight + spotSpill;

            if (spot > 0.001) {
                vec3 lightDir = -toFragDir;

                float headDiff = max(dot(normal, lightDir), 0.0);
                float nearFade = smoothstep(0.0, 1.0, distToCamera);
                float distFalloff = pow(max(0.0, 1.0 - distToCamera / headlampRange), 1.6);
                headlampContribution = headDiff * spot * distFalloff * nearFade
                                    * headlampColor * headlampIntensity;

                vec3 fakeLightPos = cameraPos + camRight * 5.0;
                vec3 fakeLightDir = normalize(worldPos - fakeLightPos);
                vec3 halfDir = normalize(-fakeLightDir + lightDir);
                headSpec = pow(max(dot(normal, halfDir), 0.0), 32.0)
                        * spot * distFalloff * 0.3;
                float ambientLoad = clamp(ambientStrength * sunVis + (1.0 - nightFactor), 0.0, 1.0);
                float headlampVisibility = 1.0 - ambientLoad;
                headlampVisibility = clamp(headlampVisibility, 0.3, 1.0);

                headlampContribution *= headlampVisibility;
                headSpec *= headlampVisibility;
            }
        }
    }

    vec3 diffuse = diff * sunColor.rgb * terrainBaseColor + headlampContribution;

    // Moonlight
    vec3 topoLuma = vec3(dot(terrainBaseColor, vec3(0.299, 0.587, 0.114)));
    vec3 nightTopoTint = mix(topoLuma, terrainBaseColor, 0.35); // partly retain hue
    vec3 moonTint = vec3(0.55, 0.68, 0.90); // keep some cool moonlight character

    float moonlightFill = nightFactor * 0.08;
    ambient += moonlightFill * mix(moonTint, nightTopoTint, 0.5) * nightTopoTint;

    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 halfDir = normalize(sunDirNorm + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 32.0) * sunVis * 0.5;

    vec3 groundColor = ambient + diffuse + spec * sunColor.rgb * 0.8 
                    + headSpec * headlampColor;

    // ================== ATMOSPHERIC PERSPECTIVE ==================
    float distNormalized = clamp(dist / farPlane, 0.0, 1.0);
    float atmosphereStrength = pow(distNormalized, 1.7);

    vec3 nightAtm = vec3(0.008, 0.006, 0.025);

    vec3 atmTint = mix(sunColor.rgb * vec3(0.7, 0.65, 0.8), 
                    vec3(0.50, 0.68, 0.95), 0.6);

    float daytimeFactor = clamp(sunDirNorm.y * 1.8, 0.0, 1.0);
    atmTint = mix(vec3(0.28, 0.18, 0.40), atmTint, daytimeFactor);
    atmTint = mix(atmTint, nightAtm, nightFactor * 0.95);

    vec3 finalColor = mix(groundColor, 
                        mix(groundColor, atmTint, 0.72), 
                        atmosphereStrength * 0.85);

    float desat = atmosphereStrength * mix(0.38, 0.75, nightFactor);
    finalColor = mix(finalColor, vec3(dot(finalColor, vec3(0.299, 0.587, 0.114))), desat);
    float normH = clamp(_h / max(u_heightMax, 1.0), 0.0, 1.0);
    float nightFloor = mix(0.32, 0.48, normH * normH); // peaks brighter at night
    finalColor *= mix(1.0, nightFloor, nightFactor);

    // ================== HEX GRID ==================
    float gridFade = pow(clamp(1.0 - (dist / farPlane), 0.0, 1.0), 2.0);

    float visibilityDist = 800.0 + camHeight * 20.0;
    float distanceFade = clamp(1.0 - (dist / visibilityDist), 0.0, 1.0);

    float gridMask = hexGrid(worldPos.xz);
    float finalGridIntensity = gridMask * gridFade * distanceFade * 0.72;

    finalColor = mix(finalColor, gridColor, finalGridIntensity);

    // ================== HEX HIGHLIGHT ==================
    if (cursorMode) {
        vec2 cell = hexAt(worldPos.xz);
        if (cell.x == hoveredHex.x && cell.y == hoveredHex.y) {
            float insideCell = 1.0 - gridMask;
            finalColor = mix(finalColor, gridColor, insideCell * 0.25);
            finalColor = mix(finalColor, gridColor, gridMask * 0.9);
        }
    }

    gl_FragColor = vec4(finalColor, 1.0);
}