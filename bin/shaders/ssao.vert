#version 460 core
out vec2 v_uv;
out vec3 v_viewRay;

layout(location = 5) uniform vec3 u_farTopRight;
layout(location = 6) uniform vec3 u_farTopLeft;
layout(location = 7) uniform vec3 u_farBottomLeft;
layout(location = 8) uniform vec3 u_farBottomRight;

void main() {
    uint id = uint(gl_VertexID);
    vec2 ndc = vec2((id << 1) & 2, id & 2) * 2.0 - 1.0;
    v_uv = ndc * 0.5 + 0.5;
    
    // Select the correct ray matching the screen corner
    if (v_uv.x < 0.5 && v_uv.y > 0.5)      v_viewRay = u_farTopLeft;
    else if (v_uv.x > 0.5 && v_uv.y > 0.5) v_viewRay = u_farTopRight;
    else if (v_uv.x < 0.5 && v_uv.y < 0.5) v_viewRay = u_farBottomLeft;
    else                                   v_viewRay = u_farBottomRight;

    gl_Position = vec4(ndc, 0.0, 1.0);
}