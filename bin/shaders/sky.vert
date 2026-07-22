#version 460 core

out vec2 v_ndc;

void main() {
    uint id = uint(gl_VertexID);
    v_ndc = vec2((id << 1) & 2, id & 2) * 2.0 - 1.0;
    
    gl_Position = vec4(v_ndc, 0.0, 1.0);
}