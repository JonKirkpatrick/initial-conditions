#version 460 core

out vec2 v_uv;

void main() {
    uint id = uint(gl_VertexID);
    v_uv = vec2((id << 1) & 2, id & 2);
    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);
}