// minimap_topo.frag

uniform vec2  u_playerXZ;          // player world position (x, z)
uniform float u_worldRadius;       // world units from center to edge (12800.0)
uniform float u_texSize;           // texture dimension in pixels (256.0)
uniform float u_heightMax;         // same as heightMax in the main shader
uniform float u_reliefExaggeration;  // set to 4.0 from C++
uniform float u_boundaryRoughness;   // 0.0 = perfect circles, ~0.15 = natural edges

// Height layer uniforms (identical to topo_topdown.frag)
uniform float u_activeLayerEnabled[16];
uniform vec2  u_layers_center[16];
uniform float u_layers_radius[16];
uniform float u_layers_falloffWidth[16];
uniform float u_layers_topoHeight[16];

float maskFromD(float d, float rd, float falloff) {
    float t = falloff;           // falloff distance

    // Inside the circle: full strength
    float inside = step(d, 0.0);  // 1.0 if d <= 0, else 0.0

    // Smooth transition zone using cosine
    float u = t - abs(d - t);
    float g = clamp(0.5 * (1.0 + u / (abs(u) - 1e-10)), 0.0, 1.0);

    float cosTerm = cos(3.141592653589793 * d / (2.0 * t));
    float b = g * ((cosTerm + 1.0) * 0.5);

    return inside + b;
}

float evaluateLayerHeightAt(in vec2 xz, int layerIdx) {
    vec2  delta  = xz - u_layers_center[layerIdx];
    float dist   = length(delta);
    float radius = u_layers_radius[layerIdx];

    float d       = dist - radius;
    float falloff = u_layers_falloffWidth[layerIdx];

    float mask = maskFromD(d, radius, falloff);
    return u_layers_topoHeight[layerIdx] * mask;
}

float heightAt(vec2 xz) {
    float height = 0.0;
    
    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] > 0.5) {
            height += evaluateLayerHeightAt(xz, i);
        }
    }
    return height;
}

// ── Analytic derivative ────────────────────────────────────────────────────

float dMaskDd(float d, float falloff) {
    float t = falloff;
    if (d <= 0.0 || d >= 2.0 * t) return 0.0;
    float arg = 3.141592653589793 * d / (2.0 * t);
    return -0.5 * sin(arg) * (3.141592653589793 / (2.0 * t));
}

vec2 evaluateLayerHeightDerivativeAt(in vec2 xz, int i) {
    vec2  delta   = xz - u_layers_center[i];
    float distSq  = dot(delta, delta);
    if (distSq < 1e-12) return vec2(0.0);

    float dist    = sqrt(distSq);
    float invDist = inversesqrt(distSq);
    float radius  = u_layers_radius[i];
    float falloff = u_layers_falloffWidth[i];

    float d       = dist - radius;

    vec2  dD_dXZ   = delta * invDist;

    float deriv = dMaskDd(d, falloff) * u_layers_topoHeight[i];
    return dD_dXZ * deriv;
}

vec3 normalAt(vec2 xz) {
    float dhdx = 0.0, dhdz = 0.0;
    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] > 0.5) {
            vec2 deriv = evaluateLayerHeightDerivativeAt(xz, i);
            dhdx += deriv.x;
            dhdz += deriv.y;
        }
    }
    vec3 n = vec3(-dhdx * u_reliefExaggeration, 1.0, -dhdz * u_reliefExaggeration);
    return n * inversesqrt(max(dot(n, n), 1e-12));
}

// Single function that returns height AND its XZ gradient in one FBm evaluation
vec3 layerHeightAndGrad(in vec2 xz, int i, float radius, float falloff, float topo) {
    vec2  delta   = xz - u_layers_center[i]; // swap prefix to layer_ for other shaders
    float distSq  = dot(delta, delta);
    if (distSq < 1e-12) return vec3(0.0);

    float dist    = sqrt(distSq);
    float invDist = inversesqrt(distSq);

    float d       = dist - radius;

    float mask    = maskFromD(d, radius, falloff);
    float height  = topo * mask;

    // Gradient — only compute if we're in the falloff zone (mask derivative non-zero)
    float maskD   = dMaskDd(d, falloff);
    vec2  dD_dXZ   = delta * invDist;
    vec2  grad     = dD_dXZ * maskD * topo;

    return vec3(height, grad.x, grad.y); // (h, dh/dx, dh/dz)
}

// Replaces both heightAt() and the normalAt() loop
void heightAndNormal(vec2 xz, out float h, out vec3 normal) {
    float totalH   = 0.0;
    float dhdx     = 0.0;
    float dhdz     = 0.0;

    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] < 0.5) continue;
        vec3 hn = layerHeightAndGrad(xz, i,
                      u_layers_radius[i],
                      u_layers_falloffWidth[i],
                      u_layers_topoHeight[i]);
        totalH += hn.x;
        dhdx   += hn.y;
        dhdz   += hn.z;
    }

    h = totalH;
    vec3 n = vec3(-dhdx * u_reliefExaggeration, 1.0, -dhdz * u_reliefExaggeration);
    normal = n * inversesqrt(max(dot(n, n), 1e-12));
}

// ── Colour palette ─────────────────────────────────────────────────────────

vec3 topoColour(float normHeight, float shade) {
    // Classic topo palette — 6 stops from lowland green to peak white
    vec3 c0 = vec3(0.467, 0.631, 0.388); // deep green       (0.00)
    vec3 c1 = vec3(0.647, 0.753, 0.447); // mid green        (0.15)
    vec3 c2 = vec3(0.827, 0.816, 0.510); // yellow-tan       (0.35)
    vec3 c3 = vec3(0.784, 0.667, 0.392); // warm ochre       (0.55)
    vec3 c4 = vec3(0.651, 0.529, 0.408); // brown            (0.72)
    vec3 c5 = vec3(0.820, 0.800, 0.788); // grey-white rock  (1.00)

    // Piecewise mix across the stops
    vec3 base;
    if      (normHeight < 0.15) base = mix(c0, c1, normHeight / 0.15);
    else if (normHeight < 0.35) base = mix(c1, c2, (normHeight - 0.15) / 0.20);
    else if (normHeight < 0.55) base = mix(c2, c3, (normHeight - 0.35) / 0.20);
    else if (normHeight < 0.72) base = mix(c3, c4, (normHeight - 0.55) / 0.17);
    else                        base = mix(c4, c5, (normHeight - 0.72) / 0.28);

    // Ambient + diffuse
    float ambient = 0.38;
    float diffuse = 0.62;
    float light   = ambient + diffuse * shade;

    // Warm/cool tint on lit vs shadowed faces
    vec3 litTint   = vec3(1.04, 1.01, 0.96);
    vec3 shadeTint = vec3(0.85, 0.88, 0.94);
    vec3 tint      = mix(shadeTint, litTint, shade);

    return clamp(base * light * tint, 0.0, 1.0);
}

// ── Main ───────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = (gl_FragCoord.xy / u_texSize) * 2.0 - 1.0;
    float r = length(uv);
    if (r > 1.0) discard;

    vec2  xz = u_playerXZ + uv * vec2(1.0, -1.0) * u_worldRadius;

    float h;
    vec3  normal;
    heightAndNormal(xz, h, normal);

    float normH  = clamp(h / max(u_heightMax, 1.0), 0.0, 1.0);
    vec3  light  = normalize(vec3(-1.0, 1.0, 1.0));
    float shade  = clamp(dot(normal, light), 0.0, 1.0);

    vec3  colour = topoColour(normH, shade);
    float edge   = smoothstep(1.0, 0.92, r);
    colour = mix(vec3(0.776, 0.902, 0.804), colour, edge);

    gl_FragColor = vec4(colour, edge);
}