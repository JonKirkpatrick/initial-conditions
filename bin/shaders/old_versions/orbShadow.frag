#version 460 core



struct OrbData {
    vec4 centreAndSpeciesIdx;               // xyz = centre,        w = species
    vec4 forwardAndRadius;                  // xyz = forward,       w = radius
    vec4 rightPadded;                       // xyz = right,         w = spare
    vec4 upPadded;                          // xyz = up,            w = spare
    vec4 gazeDirDilationAndEyelidClosure;   // xy  = gazeDir,       zw = Dilation and Eylid
};

layout(std430, binding = 0) readonly buffer OrbBuffer {
    OrbData orbs[];
};

flat in int v_instanceID;
in vec3 v_worldPos; 

uniform mat4 u_lightViewProj;
uniform vec3 u_lightDir; // Vector pointing TOWARD the sun

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct SphereHit {
    bool  hit;
    float t;
    vec3  pos;
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

    result.hit = true;
    result.t   = t;
    result.pos = ray.origin + t * ray.dir;

    return result;
}

void main()
{
    // 1. Unpack identical compound structure coordinates
    OrbData raw  = orbs[v_instanceID];
    vec3 centre  = raw.centreAndSpeciesIdx.xyz;
    vec3 forward = raw.forwardAndRadius.xyz;
    float radius = raw.forwardAndRadius.w;
    vec3 right   = raw.rightPadded.xyz;
    vec3 up      = raw.upPadded.xyz;

    // Derive eye placement geometry exactly like forward pass
    float eyeRadius   = radius * 0.22;
    float forwardPush = radius * 0.78;
    float sideSpread  = radius * 0.35;
    float verticalUp  = radius * 0.35;

    vec3 leftEyeCentre  = centre + forward * forwardPush + right * sideSpread + up * verticalUp;
    vec3 rightEyeCentre = centre + forward * forwardPush - right * sideSpread + up * verticalUp;

    // 2. Setup World Space Ray tracking along the sun vector
    Ray ray;
    ray.dir    = -normalize(u_lightDir); // Facing downstream away from the light source
    ray.origin = v_worldPos + ray.dir * 0.01; 

    // 3. Reconstruct the compound shape by testing all targets
    SphereHit bodyHit  = intersectSphere(ray, centre,          radius);
    SphereHit leftHit  = intersectSphere(ray, leftEyeCentre,   eyeRadius);
    SphereHit rightHit = intersectSphere(ray, rightEyeCentre,  eyeRadius);

    if (!bodyHit.hit && !leftHit.hit && !rightHit.hit) discard;

    // 4. Select the closest surface point encountered by the incoming light ray
    float     closestT   = 1e10;
    vec3      closestPos = vec3(0.0);

    if (bodyHit.hit  && bodyHit.t  < closestT) { closestT = bodyHit.t;  closestPos = bodyHit.pos; }
    if (leftHit.hit  && leftHit.t  < closestT) { closestT = leftHit.t;  closestPos = leftHit.pos; }
    if (rightHit.hit && rightHit.t < closestT) { closestT = rightHit.t; closestPos = rightHit.pos; }

    // 5. Output true depth mapping
    vec4 clipPos = u_lightViewProj * vec4(closestPos, 1.0);
    gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;
}