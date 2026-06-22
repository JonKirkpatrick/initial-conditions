#version 460 core

// fenestra.vert
// Projects the eight world-space corners of an orb's AABB into clip space.
// The fragment shader reconstructs the view ray from gl_FragCoord and
// camera uniforms, then intersects it with the orb's geometry.

struct OrbData {
    vec4 centreAndRadius;           // xyz = centre,        w = radius
    vec4 forwardAndDilation;        // xyz = forward,       w = dilation
    vec4 rightAndEyelidClosure;     // xyz = right,         w = eyelidClosure
    vec4 upPadded;                  // xyz = up,            w = (spare)
    vec4 gazeDirPadded;             // xy  = gazeDir,       zw = (spare — 3 floats free!)
    vec4 tapetumColourAndPresence;  // xyz = colour,        w = presence
    vec4 squashAndDirection;        // xyz = direction,     w = squashAmount
    vec4 irisAndSpeciesIdx;         // xyz = irisColour,    w = speciesRaw
};

layout(std430, binding = 0) readonly buffer OrbBuffer {
    OrbData orbs[];
};

layout(location = 0) in vec2 a_screenPos;

uniform mat4 u_viewProj;    // combined view-projection matrix

in vec3  a_worldPos;        // world-space corner of the AABB

void main()
{
    // gl_Position = u_viewProj * vec4(a_worldPos, 1.0);
    gl_Position = vec4(a_screenPos, 0.0, 1.0);
}