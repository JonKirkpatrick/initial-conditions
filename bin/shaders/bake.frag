#version 460 core

in vec2 v_worldXZ;
in vec2 v_normalXZ;

out vec4 fragColor;

void main() {
    fragColor = vec4(v_worldXZ.x, v_worldXZ.y, v_normalXZ.x, v_normalXZ.y);
}