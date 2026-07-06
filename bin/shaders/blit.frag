#version 460 core
// blit.frag
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D u_tex;
void main() {
    float depthValue = texture(u_tex, v_uv).r;
    fragColor = vec4(vec3(depthValue), 1.0);
}