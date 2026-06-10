#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2D topoTopdownTex;
uniform vec2 topdownWorldMin;
uniform vec2 topdownWorldSize;
uniform float topdownHeightMax;
uniform mat4 u_VP;

out vec2 v_worldXZ;

float decodeHeightVertex(vec2 uv) {
    vec2 sampledUV = vec2(uv.x, 1.0 - uv.y);
    vec4 c = textureLod(topoTopdownTex, sampledUV, 0.0);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    float scaled = dot(bytes, vec3(65536.0, 256.0, 1.0));
    return scaled * (topdownHeightMax / 16777215.0);
}

void main() {
    float worldX = topdownWorldMin.x + (a_uv.x * topdownWorldSize.x);
    float worldZ = topdownWorldMin.y + (a_uv.y * topdownWorldSize.y);
    float h = decodeHeightVertex(a_uv);
    
    v_worldXZ = vec2(worldX, worldZ);
    gl_Position = u_VP * vec4(worldX, h, worldZ, 1.0);
}