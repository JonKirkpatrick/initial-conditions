#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2D   u_topoTopdownTex;
uniform vec2        u_topdownWorldMin;
uniform vec2        u_topdownWorldSize;
uniform float       u_heightMax;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec2 v_worldXZ;
out vec2 v_normalXZ;   // pack just XZ, reconstruct Y in terrain.frag
out float v_worldY;

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
    v_worldY = h;

    // Central difference in UV space — one texel step
    vec2 texelSize = 1.0 / vec2(textureSize(u_topoTopdownTex, 0));
    float hL = decodeHeightVertex(a_uv + vec2(-texelSize.x, 0.0));
    float hR = decodeHeightVertex(a_uv + vec2( texelSize.x, 0.0));
    float hD = decodeHeightVertex(a_uv + vec2(0.0, -texelSize.y));
    float hU = decodeHeightVertex(a_uv + vec2(0.0,  texelSize.y));

    vec2 worldTexelSize = u_topdownWorldSize * texelSize;
    vec3 n = normalize(vec3(hL - hR, 2.0 * worldTexelSize.x, hD - hU));
    v_normalXZ = n.xz;  // Y is always positive, reconstruct in frag

    v_worldXZ = vec2(worldX, worldZ);
    gl_Position = u_Projection * (u_View * vec4(worldX, h, worldZ, 1.0));
}