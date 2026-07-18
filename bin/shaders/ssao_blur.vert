#version 460 core
out vec2 v_uv;
void main() {
    uint id = uint(gl_VertexID);
    vec2 ndc = vec2((id << 1) & 2, id & 2) * 2.0 - 1.0;
    v_uv = ndc * 0.5 + 0.5; 
    gl_Position = vec4(ndc, 0.0, 1.0);
}