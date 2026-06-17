// aabb3d.glsl
// Axis-aligned bounding box primitive.
// Provides world-to-local mapping and ray intersection as a foundation
// for rendering geometry within a 3D volume in a fragment shader.

struct AABB {
    vec3 min;
    vec3 max;
};

// Construct an AABB from a centre point and uniform radius
AABB aabbFromCentreRadius(vec3 centre, float radius) {
    return AABB(centre - vec3(radius), centre + vec3(radius));
}

// Construct an AABB from a centre point and per-axis half extents
// Useful for ellipsoids or non-uniform bounding volumes
AABB aabbFromCentreExtents(vec3 centre, vec3 halfExtents) {
    return AABB(centre - halfExtents, centre + halfExtents);
}

// Ray-AABB intersection using the slab method.
// Returns true if the ray hits the box.
// t0 and t1 are the entry and exit distances along the ray.
// A hit requires t0 < t1 and t1 > 0 (box is in front of ray origin).
bool intersectAABB(vec3 rayOrigin, vec3 rayDir, AABB box, out float t0, out float t1) {
    vec3 invDir = 1.0 / rayDir;
    vec3 tMin   = (box.min - rayOrigin) * invDir;
    vec3 tMax   = (box.max - rayOrigin) * invDir;

    vec3 tEnter = min(tMin, tMax);
    vec3 tExit  = max(tMin, tMax);

    t0 = max(max(tEnter.x, tEnter.y), tEnter.z);  // furthest entry plane
    t1 = min(min(tExit.x,  tExit.y),  tExit.z);   // nearest exit plane

    return t1 > 0.0 && t0 < t1;
}

// Convert a world-space point inside the AABB to normalised local coordinates.
// Returns [-1, 1] on each axis, where 0 is the centre of the box.
// Assumes point is inside the box — no clamping applied.
vec3 aabbWorldToLocal(vec3 worldPoint, AABB box) {
    vec3 centre     = (box.min + box.max) * 0.5;
    vec3 halfExtents = (box.max - box.min) * 0.5;
    return (worldPoint - centre) / halfExtents;
}

// Convert a local [-1, 1] coordinate back to world space.
vec3 aabbLocalToWorld(vec3 localPoint, AABB box) {
    vec3 centre      = (box.min + box.max) * 0.5;
    vec3 halfExtents = (box.max - box.min) * 0.5;
    return centre + localPoint * halfExtents;
}

// Returns the centre of the AABB.
vec3 aabbCentre(AABB box) {
    return (box.min + box.max) * 0.5;
}

// Returns the half extents of the AABB on each axis.
vec3 aabbHalfExtents(AABB box) {
    return (box.max - box.min) * 0.5;
}