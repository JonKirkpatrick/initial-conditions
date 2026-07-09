// ===============================================================================
// == orbCompound.glsl ===========================================================
// == This orb morphology's compound shape: one body sphere + two eye spheres.  ==
// == Both orbCreature.frag and orbShadow.frag need to derive the same eye      ==
// == placement and run the same three-sphere/closest-hit test.  This file is   ==
// == the single source of truth for that, so the two passes can never drift.   ==
// ==                                                                           ==
// == NOTE: a future orb species with different morphology (e.g. no eyes, or    ==
// == more limbs) is expected to get its own orb<Type>Compound.glsl alongside   ==
// == this one, built on top of common/raySphere.glsl. Only the primitives in   ==
// == raySphere.glsl are meant to be universal — this file is specific to       ==
// == *this* creature's shape.                                                  ==
// ==                                                                           ==
// == Requires common/raySphere.glsl and orb/orbData.glsl to be included        ==
// == beforehand.                                                               ==
// ===============================================================================
#ifndef ORB_COMPOUND_GLSL
#define ORB_COMPOUND_GLSL

struct OrbEyeGeometry {
    vec3  leftEyeCentre;
    vec3  rightEyeCentre;
    float eyeRadius;
};

OrbEyeGeometry deriveOrbEyeGeometry(vec3 centre, vec3 forward, vec3 right, vec3 up, float radius)
{
    OrbEyeGeometry eyes;

    eyes.eyeRadius     = radius * 0.22;
    float forwardPush  = radius * 0.78;
    float sideSpread   = radius * 0.35;
    float verticalUp   = radius * 0.35;

    eyes.leftEyeCentre  = centre + forward * forwardPush + right * sideSpread + up * verticalUp;
    eyes.rightEyeCentre = centre + forward * forwardPush - right * sideSpread + up * verticalUp;

    return eyes;
}

// hitType: 0 = body, 1 = left eye, 2 = right eye
struct OrbCompoundHit {
    SphereHit hit;
    int       hitType;
};

OrbCompoundHit intersectOrbCompound(Ray ray, vec3 centre, float radius, OrbEyeGeometry eyes)
{
    SphereHit bodyHit  = intersectSphere(ray, centre,             radius);
    SphereHit leftHit  = intersectSphere(ray, eyes.leftEyeCentre,  eyes.eyeRadius);
    SphereHit rightHit = intersectSphere(ray, eyes.rightEyeCentre, eyes.eyeRadius);

    OrbCompoundHit result;
    result.hit.hit = false;
    result.hitType = -1;

    float closestT = 1e10;

    if (bodyHit.hit  && bodyHit.t  < closestT) { closestT = bodyHit.t;  result.hit = bodyHit;  result.hitType = 0; }
    if (leftHit.hit  && leftHit.t  < closestT) { closestT = leftHit.t;  result.hit = leftHit;  result.hitType = 1; }
    if (rightHit.hit && rightHit.t < closestT) { closestT = rightHit.t; result.hit = rightHit; result.hitType = 2; }

    return result;
}

#endif // ORB_COMPOUND_GLSL
