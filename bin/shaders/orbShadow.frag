#version 460 core

struct OrbData {
    vec4 centreAndSpeciesIdx;
    vec4 forwardAndRadius;
    vec4 rightPadded;
    vec4 upPadded;
    vec4 gazeDirDilationAndEyelidClosure;
};

layout(std430, binding = 0) readonly buffer OrbBuffer {
    OrbData orbs[];
};

flat in int v_instanceID;
in vec3 v_proxyWorldPos;

uniform mat4 u_shadowMatrix; // Active cascade View-Projection
uniform vec3 u_sunDir;       // Direction the sun is shining

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct SphereHit {
    bool  hit;
    float t;
    vec3  pos;
};

SphereHit intersectSphere(Ray ray, vec3 centre, float radius) {
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

    return result;
}

void main() {
    // 1. Unpack orb configuration mirroring orbCreature.frag
    OrbData raw = orbs[v_instanceID];
    vec3  centre      = raw.centreAndSpeciesIdx.xyz;
    float radius      = raw.forwardAndRadius.w;
    vec3  forward     = raw.forwardAndRadius.xyz;
    vec3  right       = raw.rightPadded.xyz;
    vec3  up          = raw.upPadded.xyz;
    float eyelidClose = raw.gazeDirDilationAndEyelidClosure.w;
    vec2  gazeDirRaw  = raw.gazeDirDilationAndEyelidClosure.xy;

    float maxGazeSpread = 0.57735027;
    vec3  gazeTarget    = forward + (gazeDirRaw.x * maxGazeSpread * right) + (gazeDirRaw.y * maxGazeSpread * up);
    vec3  gazeDir       = normalize(gazeTarget);

    float eyeRadius   = radius * 0.22;
    float forwardPush = radius * 0.78;
    float sideSpread  = radius * 0.35;
    float verticalUp  = radius * 0.35;
    vec3  leftEyeCentre  = centre + forward * forwardPush + right * sideSpread + up * verticalUp;
    vec3  rightEyeCentre = centre + forward * forwardPush - right * sideSpread + up * verticalUp;

    // 2. Set up parallel ray tracing from the Sun's direction
    // The ray originates from the proxy mesh surface pointing along the sun's path
    Ray ray;
    ray.dir    = normalize(u_sunDir);
    ray.origin = v_proxyWorldPos - ray.dir * (radius * 2.0); // Pull back along ray to catch front intersections

    // 3. Evaluate hits
    SphereHit bodyHit  = intersectSphere(ray, centre,         radius);
    SphereHit leftHit  = intersectSphere(ray, leftEyeCentre,  eyeRadius);
    SphereHit rightHit = intersectSphere(ray, rightEyeCentre, eyeRadius);

    if (!bodyHit.hit && !leftHit.hit && !rightHit.hit) discard;

    // Find closest hit point relative to the sun
    float closestT = 1e10;
    vec3  finalHitPos = vec3(0.0);

    if (bodyHit.hit  && bodyHit.t  < closestT) { closestT = bodyHit.t;  finalHitPos = bodyHit.pos; }
    if (leftHit.hit  && leftHit.t  < closestT) { closestT = leftHit.t;  finalHitPos = leftHit.pos; }
    if (rightHit.hit && rightHit.t < closestT) { closestT = rightHit.t; finalHitPos = rightHit.pos; }

    // 4. Project the true hit position to light clip space to write gl_FragDepth
    vec4 clipPos = u_shadowMatrix * vec4(finalHitPos, 1.0);
    float ndcZ   = clipPos.z / clipPos.w;
    gl_FragDepth = ndcZ * 0.5 + 0.5;
}