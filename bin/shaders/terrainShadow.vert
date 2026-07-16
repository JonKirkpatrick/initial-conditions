#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2D u_topoTopdownTex;
uniform vec2      u_topdownWorldMin;
uniform vec2      u_topdownWorldSize;
uniform float     u_heightMax;
uniform mat4      u_lightViewProj;

float decodeHeightVertex(vec2 uv) {
    // With GL_R32F, the GPU hardware automatically reads and interpolates
    // the raw height float in the red channel.
    return textureLod(u_topoTopdownTex, uv, 0.0).r;
}

void main() {
    float worldX = u_topdownWorldMin.x + (a_uv.x * u_topdownWorldSize.x);
    float worldZ = u_topdownWorldMin.y + (a_uv.y * u_topdownWorldSize.y);
    float h = decodeHeightVertex(a_uv);

    gl_Position = u_lightViewProj * vec4(worldX, h, worldZ, 1.0);
}