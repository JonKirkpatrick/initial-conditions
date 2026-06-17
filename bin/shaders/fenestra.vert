#version 460 core

// fenestra.vert
// Projects the eight world-space corners of an orb's AABB into clip space.
// The orb index is encoded in the vertex color and interpolated across
// the cube faces, giving each fragment knowledge of which orb it belongs to.
// The fragment shader reconstructs the view ray from gl_FragCoord and
// camera uniforms, then intersects it with the orb's geometry.

layout(location = 0) in vec2 a_screenPos;

uniform mat4 u_viewProj;    // combined view-projection matrix

in vec3  a_worldPos;        // world-space corner of the AABB
in vec4  a_color;           // orb index encoded as color (matches current convention)

out vec4 v_color;           // passed through to fragment shader

void main()
{
    v_color     = a_color;
    // gl_Position = u_viewProj * vec4(a_worldPos, 1.0);
    gl_Position = vec4(a_screenPos, 0.0, 1.0);
}