#version 460 core

uniform vec2 topdownWorldMin;
uniform vec2 topdownWorldSize;
uniform sampler2D topoTopdownTex;


uniform vec2  u_playerXZ;
uniform float u_worldRadius;
uniform float u_texSize;
uniform float u_heightMax;

out vec4 fragColor;

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

// Helper: raw height at integer texel (nearest only)
float rawHeightAt(ivec2 p) {
    vec4 c = texelFetch(topoTopdownTex, p, 0);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    float scaled = dot(bytes, vec3(65536.0, 256.0, 1.0));
    return scaled * (u_heightMax / 16777215.0);
}


// ================== XZ DECODE ==================
vec2 decodeXZ(vec4 c) {
    float hiX = floor(c.r * 255.0 + 0.5);
    float loX = floor(c.g * 255.0 + 0.5);
    float hiZ = floor(c.b * 255.0 + 0.5);
    float loZ = floor(c.a * 255.0 + 0.5);
    float normX = (hiX * 256.0 + loX) / 65535.0;
    float normZ = (hiZ * 256.0 + loZ) / 65535.0;
    return topdownWorldMin + vec2(normX, normZ) * topdownWorldSize;
}

// ================== HEIGHT from topdown texture (Bilinear) ==================
float decodeHeight(vec2 xz) {
    vec2 uv = (xz - topdownWorldMin) / topdownWorldSize;
    uv.y = 1.0 - uv.y;
    
    // Convert to texel space
    vec2 texSize = vec2(textureSize(topoTopdownTex, 0));
    vec2 px = uv * (texSize - 1.0);
    
    ivec2 p0 = ivec2(floor(px));
    vec2 f = fract(px); 
    
    float h00 = rawHeightAt(p0);
    float h10 = rawHeightAt(p0 + ivec2(1, 0));
    float h01 = rawHeightAt(p0 + ivec2(0, 1));
    float h11 = rawHeightAt(p0 + ivec2(1, 1));
    
    // Bilinear interpolation
    float h0 = mix(h00, h10, f.x);
    float h1 = mix(h01, h11, f.x);
    return mix(h0, h1, f.y);
}

// ================== NORMAL from topdown texture ==================
vec3 computeNormal(vec2 xz) {
    vec2 texSize = vec2(textureSize(topoTopdownTex, 0));
    float eps = topdownWorldSize.x / texSize.x;
    
    float hL = decodeHeight(xz + vec2(-eps, 0.0));
    float hR = decodeHeight(xz + vec2( eps, 0.0));
    float hD = decodeHeight(xz + vec2(0.0, -eps));
    float hU = decodeHeight(xz + vec2(0.0,  eps));

    return normalize(vec3(hL - hR, 2.0 * eps, hD - hU));
}

// ── Main ───────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = (gl_FragCoord.xy / u_texSize) * 2.0 - 1.0;
    float r = length(uv);
    if (r > 1.0) discard;

    vec2  xz = u_playerXZ + uv * vec2(1.0, -1.0) * u_worldRadius;

    float h = decodeHeight(xz);
    vec3  normal = computeNormal(xz);

    float normH  = clamp(h / max(u_heightMax, 1.0), 0.0, 1.0);
    vec3  light  = normalize(vec3(-1.0, 1.0, 1.0));
    float shade  = clamp(dot(normal, light), 0.0, 1.0);

    vec3  colour = topoColour(normH, shade);
    float edge   = smoothstep(1.0, 0.92, r);
    colour = mix(vec3(0.776, 0.902, 0.804), colour, edge);

    fragColor = vec4(colour, edge);
}