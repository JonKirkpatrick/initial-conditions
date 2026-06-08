#version 460 core

layout(location = 0) in vec2 a_xz;

uniform sampler2D topoTopdownTex;
uniform vec2 topdownWorldMin;
uniform vec2 topdownWorldSize;
uniform float topdownHeightMax;
uniform mat4 u_VP;

out vec2 v_xz;

float decodeHeight(vec2 xz) {
    vec2 uv = (xz - topdownWorldMin) / topdownWorldSize;
    uv.y = 1.0 - uv.y;
    vec4 c = texture(topoTopdownTex, uv);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    float scaled = dot(bytes, vec3(65536.0, 256.0, 1.0));
    return scaled * (topdownHeightMax / 16777215.0);
}

void main() {
    float h = decodeHeight(a_xz);
    v_xz = a_xz;
    gl_Position = u_VP * vec4(a_xz.x, h, a_xz.y, 1.0);
}