// minimap_topo.frag

uniform vec2  u_playerXZ;          // player world position (x, z)
uniform float u_worldRadius;       // world units from center to edge (12800.0)
uniform float u_texSize;           // texture dimension in pixels (256.0)
uniform float u_heightMax;         // same as heightMax in the main shader
uniform float u_reliefExaggeration;  // set to 4.0 from C++

// Height layer uniforms (identical to topo_topdown.frag)
uniform float u_activeLayerEnabled[16];
uniform vec2  u_layers_center[16];
uniform float u_layers_radius[16];
uniform float u_layers_falloffWidth[16];
uniform float u_layers_topoHeight[16];
// Map the existing minimap uniforms onto the shared terrain helper names.
#define layer_center u_layers_center
#define layer_radius u_layers_radius
#define layer_falloffWidth u_layers_falloffWidth
#define layer_topoHeight u_layers_topoHeight

#include "topo_common.glsl"

#undef layer_center
#undef layer_radius
#undef layer_falloffWidth
#undef layer_topoHeight

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
    normal = normalize(vec3(normal.x * u_reliefExaggeration, normal.y, normal.z * u_reliefExaggeration));

    float normH  = clamp(h / max(u_heightMax, 1.0), 0.0, 1.0);
    vec3  light  = normalize(vec3(-1.0, 1.0, 1.0));
    float shade  = clamp(dot(normal, light), 0.0, 1.0);

    vec3  colour = topoColour(normH, shade);
    float edge   = smoothstep(1.0, 0.92, r);
    colour = mix(vec3(0.776, 0.902, 0.804), colour, edge);

    gl_FragColor = vec4(colour, edge);
}