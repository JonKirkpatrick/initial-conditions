#version 460 core

#include "common/raySphere.glsl"
#include "orb/orbData.glsl"
#include "orb/orbCompound.glsl"
#include "ubos/environment.glsl"

flat in int v_instanceID;
in vec3 v_worldPos;

uniform mat4 u_lightViewProj;

void main()
{
    OrbData raw  = orbs[v_instanceID];
    vec3 centre  = raw.centreAndSpeciesIdx.xyz;
    vec3 forward = raw.forwardAndRadius.xyz;
    float radius = raw.forwardAndRadius.w;
    vec3 right   = raw.rightPadded.xyz;
    vec3 up      = raw.upPadded.xyz;

    OrbEyeGeometry eyes = deriveOrbEyeGeometry(centre, forward, right, up, radius);

    Ray ray;
    ray.dir    = -normalize(u_sunDir); // Using the UBO sun direction variable now!
    ray.origin = v_worldPos + ray.dir * 0.01;

    OrbCompoundHit compound = intersectOrbCompound(ray, centre, radius, eyes);

    if (!compound.hit.hit) discard;

    vec4 clipPos = u_lightViewProj * vec4(compound.hit.pos, 1.0);
    gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;
}