#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2D u_topoTopdownTex;
uniform vec2      u_topdownWorldMin;
uniform vec2      u_topdownWorldSize;
uniform float     u_heightMax;
uniform mat4      u_lightViewProj;

float decodeHeightVertex(vec2 uv) {
    vec2 s = vec2(uv.x, 1.0 - uv.y);
    vec4 c = textureLod(u_topoTopdownTex, s, 0.0);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    return dot(bytes, vec3(65536.0, 256.0, 1.0)) * (u_heightMax / 16777215.0);
}

void main() {
    float worldX = u_topdownWorldMin.x + (a_uv.x * u_topdownWorldSize.x);
    float worldZ = u_topdownWorldMin.y + (a_uv.y * u_topdownWorldSize.y);
    float h = decodeHeightVertex(a_uv);

    gl_Position = u_lightViewProj * vec4(worldX, h, worldZ, 1.0);
}