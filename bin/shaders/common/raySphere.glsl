// ==============================================================================
// == raySphere.glsl ============================================================
// == Generic ray/sphere primitives. No knowledge of orbs, species, or any     ==
// == particular compound shape lives here.  This is the truly reusable layer, ==
// == safe to pull into terrain shaders or any future creature's passes.       ==
// ==============================================================================
#ifndef RAYSPHERE_GLSL
#define RAYSPHERE_GLSL

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct SphereHit {
    bool  hit;
    float t;
    vec3  pos;
    vec3  normal;
};

SphereHit intersectSphere(Ray ray, vec3 centre, float radius)
{
    SphereHit result;
    result.hit = false;

    vec3  oc           = ray.origin - centre;
    float a            = dot(ray.dir, ray.dir);
    float b            = 2.0 * dot(oc, ray.dir);
    float c            = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0.0) return result;

    float sqrtD = sqrt(discriminant);
    float t0    = (-b - sqrtD) / (2.0 * a);
    float t1    = (-b + sqrtD) / (2.0 * a);

    float t = (t0 > 0.0) ? t0 : t1;
    if (t <= 0.0) return result;

    result.hit    = true;
    result.t      = t;
    result.pos    = ray.origin + t * ray.dir;
    result.normal = normalize(result.pos - centre);

    return result;
}

#endif // RAYSPHERE_GLSL