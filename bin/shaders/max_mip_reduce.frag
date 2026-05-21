uniform vec2 viewportSize;
uniform sampler2D sourceTex;
uniform vec2 sourceTexelSize;
uniform float heightMax;

vec3 packHeight24(float heightValue, float maxHeightValue) {
    float normalized = clamp(heightValue / max(maxHeightValue, 1e-6), 0.0, 1.0);
    float scaled = normalized * 16777215.0;
    float r = floor(scaled / 65536.0);
    float g = floor((scaled - r * 65536.0) / 256.0);
    float b = floor(scaled - r * 65536.0 - g * 256.0);
    return vec3(r, g, b) / 255.0;
}

float decodeHeight24(vec4 color, float maxHeightValue) {
    float packed = color.r * 65536.0 * 255.0 + color.g * 256.0 * 255.0 + color.b * 255.0;
    return (packed / 16777215.0) * max(maxHeightValue, 1e-6);
}

void main() {
    vec2 outCoord = gl_FragCoord.xy - vec2(0.5);
    vec2 srcBase = outCoord * 2.0 + vec2(0.5);

    vec2 uv00 = srcBase * sourceTexelSize;
    vec2 uv10 = (srcBase + vec2(1.0, 0.0)) * sourceTexelSize;
    vec2 uv01 = (srcBase + vec2(0.0, 1.0)) * sourceTexelSize;
    vec2 uv11 = (srcBase + vec2(1.0, 1.0)) * sourceTexelSize;

    float h00 = decodeHeight24(texture2D(sourceTex, uv00), heightMax);
    float h10 = decodeHeight24(texture2D(sourceTex, uv10), heightMax);
    float h01 = decodeHeight24(texture2D(sourceTex, uv01), heightMax);
    float h11 = decodeHeight24(texture2D(sourceTex, uv11), heightMax);

    float h = max(max(h00, h10), max(h01, h11));
    gl_FragColor = vec4(packHeight24(h, heightMax), 1.0);
}
