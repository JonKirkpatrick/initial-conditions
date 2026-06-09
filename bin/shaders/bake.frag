#version 460 core

in vec2 v_xz;

uniform vec2 topdownWorldMin;
uniform vec2 topdownWorldSize;

out vec4 fragColor;

void main() {
    // Normalize XZ to 0..1 within world bounds
    vec2 norm = (v_xz - topdownWorldMin) / topdownWorldSize;

    // Pack each axis across two bytes for ~2.3cm precision
    // X in RG, Z in BA
    norm = clamp(norm, 0.0, 1.0);
    norm = min(norm, 0.99999994);

    vec2 scaled = norm * 65535.0;
    vec2 hi = floor(scaled / 256.0);
    vec2 lo = floor(scaled) - hi * 256.0;

    fragColor = vec4(hi.x, lo.x, hi.y, lo.y) / 255.0;
}