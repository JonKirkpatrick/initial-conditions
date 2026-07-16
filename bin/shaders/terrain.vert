#version 460 core

layout(location = 0) in vec2 a_uv;

uniform sampler2D   u_topoTopdownTex;
uniform vec2        u_topdownWorldMin;
uniform vec2        u_topdownWorldSize;
uniform float       u_heightMax;

// Pass the unified, native view-projection matrix to match your C++ loop format perfectly
uniform mat4 u_viewProj; 

out vec2 v_worldXZ;
out vec2 v_normalXZ;   
out float v_worldY;

float decodeHeightVertex(vec2 uv) {
    // With GL_R32F, the GPU hardware automatically reads and interpolates
    // the raw height float in the red channel.
    return textureLod(u_topoTopdownTex, uv, 0.0).r;
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
    
    // Standard Right-Handed Surface Gradient Normal
    vec3 n = normalize(vec3(hL - hR, 2.0 * worldTexelSize.x, hD - hU));
    
    // Save the raw X and Z derivative components clearly
    v_normalXZ = vec2(n.x, n.z);  

    v_worldXZ = vec2(worldX, worldZ);
    
    // Single unified transformation protects matrix hierarchy from cross-multiplying out of order
    gl_Position = u_viewProj * vec4(worldX, h, worldZ, 1.0);
}