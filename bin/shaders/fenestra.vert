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
    vec4 gazeDirPadded;             // xy  = gazeDir,       zw = (spare)
    vec4 tapetumColourAndPresence;  // xyz = colour,        w = presence
    vec4 squashAndDirection;        // xyz = direction,     w = squashAmount
    vec4 irisAndSpeciesIdx;         // xyz = irisColour,    w = speciesRaw
};

layout(std430, binding = 0) readonly buffer OrbBuffer {
    OrbData orbs[];
};

layout(location = 0) in vec3 a_cubePos;

uniform mat4 u_viewProj;

flat out int v_instanceID;

void main()
{
    OrbData orb    = orbs[gl_InstanceID];
    vec3 centre    = orb.centreAndRadius.xyz;
    float radius   = orb.centreAndRadius.w;

    // Scale the unit cube to the orb's radius and translate to its centre
    vec3 worldPos  = a_cubePos * radius + centre;

    v_instanceID   = gl_InstanceID;
    gl_Position    = u_viewProj * vec4(worldPos, 1.0);
}