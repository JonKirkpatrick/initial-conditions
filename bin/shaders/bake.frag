#version 460 core

in vec2 v_worldXZ;

out vec4 fragColor;

void main() {
    fragColor = vec4(v_worldXZ.x, v_worldXZ.y, 0.0, 1.0);
}