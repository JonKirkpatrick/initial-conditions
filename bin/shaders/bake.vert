#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2D topoTopdownTex;
uniform vec2 topdownWorldMin;
uniform vec2 topdownWorldSize;
uniform float topdownHeightMax;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec2 v_worldXZ;
out vec2 v_normalXZ;   // pack just XZ, reconstruct Y in terrain.frag

float decodeHeightVertex(vec2 uv) {
    vec2 s = vec2(uv.x, 1.0 - uv.y);
    vec4 c = textureLod(topoTopdownTex, s, 0.0);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    return dot(bytes, vec3(65536.0, 256.0, 1.0)) * (topdownHeightMax / 16777215.0);
}

void main() {
    float worldX = topdownWorldMin.x + (a_uv.x * topdownWorldSize.x);
    float worldZ = topdownWorldMin.y + (a_uv.y * topdownWorldSize.y);
    float h = decodeHeightVertex(a_uv);

    // Central difference in UV space — one texel step
    vec2 texelSize = 1.0 / vec2(textureSize(topoTopdownTex, 0));
    float hL = decodeHeightVertex(a_uv + vec2(-texelSize.x, 0.0));
    float hR = decodeHeightVertex(a_uv + vec2( texelSize.x, 0.0));
    float hD = decodeHeightVertex(a_uv + vec2(0.0, -texelSize.y));
    float hU = decodeHeightVertex(a_uv + vec2(0.0,  texelSize.y));

    vec2 worldTexelSize = topdownWorldSize * texelSize;
    vec3 n = normalize(vec3(hL - hR, 2.0 * worldTexelSize.x, hD - hU));
    v_normalXZ = n.xz;  // Y is always positive, reconstruct in frag

    v_worldXZ = vec2(worldX, worldZ);
    gl_Position = u_Projection * (u_View * vec4(worldX, h, worldZ, 1.0));
}