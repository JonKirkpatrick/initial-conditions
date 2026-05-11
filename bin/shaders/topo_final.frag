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

// ================== SLOPE-BASED TERRAIN COLORING ==================
vec3 getTerrainColor(vec3 normal, vec3 baseColor) {
    // Grass on flatter areas, baseColor (rock/dirt) on steeper slopes
    vec3 grassColor = vec3(0.18, 0.48, 0.12);     // Nice natural grass green - tune as desired
    
    float slopeThreshold = 0.98;   // Higher = grass only on very flat areas
    float blendSharpness = 0.08;   // Controls how gradual the transition is
    
    // normal.y = 1.0 on perfectly flat, decreases as slope increases
    float flatness = smoothstep(slopeThreshold - blendSharpness, 
                                slopeThreshold + blendSharpness, 
                                normal.y);
    
    return mix(baseColor, grassColor, flatness);
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

float decodeTopoDepth(vec4 topo) {
    return topo.r + topo.g / 255.0 + topo.b / (255.0 * 255.0);
}

// Simple hash helper (inlined from sky.frag) to avoid cross-shader dependency
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 skyColorForRay(vec3 rayDir) {
    vec3 rayNorm = normalize(rayDir);
    vec3 sunDirNorm = normalize(sunDir);
    float sunDot = max(dot(rayNorm, sunDirNorm), 0.0);
    float height = rayNorm.y;
    float sunElevation = asin(sunDirNorm.y) * 180.0 / 3.14159265;

    float dayFactor      = smoothstep(0.0, 10.0, sunElevation);
    float nightFactor    = smoothstep(-5.0, -12.0, sunElevation);
    float twilightFactor = clamp(1.0 - dayFactor - nightFactor, 0.0, 1.0);

    vec3 dayZenith      = vec3(0.25, 0.45, 0.95);
    vec3 dayHorizon     = vec3(0.70, 0.80, 0.95);
    vec3 twilightZenith  = vec3(0.45, 0.25, 0.65);
    vec3 twilightHorizon = vec3(0.95, 0.55, 0.35);
    vec3 nightColor     = vec3(0.02, 0.01, 0.08);

    float tDay      = pow((height + 1.0) * 0.5, 1.4);
    float tTwilight = pow((height + 1.0) * 0.5, 1.6);

    vec3 daySky      = mix(dayHorizon, dayZenith, tDay);
    vec3 twilightSky = mix(twilightHorizon, twilightZenith, tTwilight);
    vec3 skyColor = daySky * dayFactor
                  + twilightSky * twilightFactor
                  + nightColor * nightFactor;

    float golden = pow(sunDot, 8.0) * 0.8 * twilightFactor;
    skyColor += golden * vec3(1.0, 0.7, 0.4);

    float sunGlow = pow(sunDot, 24.0) * 3.5;
    vec3 sunLight = sunColor.rgb * sunColor.a;
    skyColor += sunGlow * sunLight * 0.9;

    float discFactor = smoothstep(-2.0, 2.0, sunElevation);
    float sunDisc = pow(sunDot, 180.0);
    skyColor += sunDisc * vec3(1.0, 0.95, 0.82) * 2.0 * discFactor;

    float starFactor = smoothstep(-3.0, -10.0, sunElevation);
    vec2 starUV = rayNorm.xz / (rayNorm.y + 1.01);
    vec2 cell = floor(starUV * 800.0);
    float h = hash(cell);
    float starMask = step(0.9975, h);
    float starBrightness = pow(h, 12.0);
    skyColor += starMask * starBrightness * vec3(0.9, 0.95, 1.0) * starFactor;

    float haze = pow(1.0 - clamp(height, 0.0, 1.0), 4.0) * 0.7;
    skyColor += haze * vec3(0.8, 0.65, 0.55) * 0.6;

    return skyColor;
}

// ================== MAIN ==================
void main() {
    vec2 uv = gl_FragCoord.xy / viewportSize;

    vec4 topoC = texture2D(topoTex, uv);

    float hitC = topoC.a >= 0.5 ? 1.0 : 0.0;

    float coverage = hitC;
    float terrainAlpha = smoothstep(0.12, 0.88, coverage);

    float dist = 0.0;
    vec3 worldPos;
    bool isTerrainHit = (hitC > 0.0);

    if (isTerrainHit) {
        float dC = decodeTopoDepth(topoC);
        float d = dC;
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
        vec2 screen = gl_FragCoord.xy;
        screen.y = viewportSize.y - screen.y;
        float x_ndc = (screen.x / viewportSize.x) * 2.0 - 1.0;
        float y_ndc = 1.0 - (screen.y / viewportSize.y) * 2.0;

        float f = tan(fovY * 0.5);
        vec3 rayDir = normalize(invRotationMatrix * vec3(x_ndc * f * aspectRatio, y_ndc * f, -1.0));

        if (rayDir.y >= -0.001) {
            gl_FragColor = vec4(skyColorForRay(rayDir), 1.0);
            return;
        }

        float t = -cameraPos.y / rayDir.y;
        if (t < nearPlane || t > farPlane * 1.3) {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }

        dist = t;
        worldPos = cameraPos + rayDir * t;
        worldPos.y = length(worldPos.xz) * 0.000008;
    }

    // ================== LIGHTING ==================
    float _h; // height not needed here, normal is what matters
    vec3 normal;
    if (isTerrainHit) {
        heightAndNormal(worldPos.xz, _h, normal);
    } else {
        normal = vec3(0.0, 1.0, 0.0);
    }

    // === SLOPE COLORING ===
    vec3 terrainBaseColor = isTerrainHit ? 
                            getTerrainColor(normal, baseColor) : 
                            baseColor;

    vec3 sunDirNorm = normalize(sunDir);
    float sunElevationDeg = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;

    float nightFactor = smoothstep(-5.0, -15.0, sunElevationDeg);
    float sunVis = smoothstep(-4.0, 8.0, sunElevationDeg);

    float diff = max(dot(normal, sunDirNorm), 0.0) * sunVis;

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
    float moonlightFill = nightFactor * 0.08;
    ambient += moonlightFill * vec3(0.6, 0.75, 1.0) * terrainBaseColor;

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
    finalColor *= mix(1.0, 0.35, nightFactor);

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