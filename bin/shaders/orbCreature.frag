#version 460 core

// ==============================================================================
// == SSBO Layout ===============================================================
// ==============================================================================

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

struct SpeciesData {
    vec4 irisColourAndRadius;               // xyz = irisColour,    w = irisRadius
    vec4 scleraColour;                      // xyz = scleraColour,  w = spare
    vec4 tapetumColourAndPresence;          // xyz = tepetumColour, w = presence (0 or 1)
};

layout(std430, binding = 1) readonly buffer SpeciesBuffer {
    SpeciesData species[];
};

flat in int v_instanceID;

// ==============================================================================
// == Uniforms ==================================================================
// ==============================================================================

// Texture samples
uniform sampler2DArray u_charDiffuseTex;
uniform sampler2DArray u_charNormalTex;

// Camera uniforms
uniform vec2      u_viewportSize;
uniform float     u_fovY;
uniform vec3      u_cameraPos;
uniform vec3      u_cameraRight;
uniform vec3      u_cameraUp;
uniform vec3      u_cameraForward;

// Light uniforms
uniform vec3      u_sunDir;
uniform vec4      u_sunColour;
uniform float     u_headlampIntensity;
uniform float     u_headlampRange;
uniform float     u_headlampCone;       // cos(angle) of headlamp cone for cutoff
uniform float     u_headlampEnabled;

out vec4 fragColour;

// ==============================================================================
// == OrbInstance — unpacked, derived per-instance data =========================
// ==============================================================================

struct OrbInstance {
    // Directly unpacked from OrbData
    vec3  centre;
    float speciesRaw;
    vec3  forward;
    float radius;
    vec3  right;
    vec3  up;
    vec3  gazeDir;
    float dilation;
    float eyelidClosure;

    // Derived geometry
    vec3  leftEyeCentre;
    vec3  rightEyeCentre;
    float eyeRadius;

    // Pulled from Species SSBO
    vec3  irisColour;
    float irisRadius;
    vec3  scleraColour;
    vec3  tapetumColour;
    float tapetumPresence;
};

OrbInstance unpackOrb(int instanceID)
{
    OrbData raw = orbs[instanceID];
    OrbInstance o;

    o.centre          = raw.centreAndSpeciesIdx.xyz;
    o.speciesRaw      = raw.centreAndSpeciesIdx.w;
    o.forward         = raw.forwardAndRadius.xyz;
    o.radius          = raw.forwardAndRadius.w;
    o.dilation        = raw.gazeDirDilationAndEyelidClosure.z;
    o.right           = raw.rightPadded.xyz;
    o.eyelidClosure   = raw.gazeDirDilationAndEyelidClosure.w;
    o.up              = raw.upPadded.xyz;

    // Decode gaze direction from compact 2D representation
    float maxGazeSpread = 0.57735027;
    vec2  gazeDirRaw    = raw.gazeDirDilationAndEyelidClosure.xy;
    vec3  gazeTarget    = o.forward
                        + (gazeDirRaw.x * maxGazeSpread * o.right)
                        + (gazeDirRaw.y * maxGazeSpread * o.up);
    o.gazeDir = normalize(gazeTarget);

    // Derive eye placement geometry
    o.eyeRadius = o.radius * 0.22;
    float forwardPush = o.radius * 0.78;
    float sideSpread  = o.radius * 0.35;
    float verticalUp  = o.radius * 0.35;

    o.leftEyeCentre  = o.centre + o.forward * forwardPush + o.right * sideSpread + o.up * verticalUp;
    o.rightEyeCentre = o.centre + o.forward * forwardPush - o.right * sideSpread + o.up * verticalUp;

    // Pull species data
    int speciesIdx = int(o.speciesRaw);
    SpeciesData sp = species[speciesIdx];

    o.irisColour      = sp.irisColourAndRadius.xyz;
    o.irisRadius      = sp.irisColourAndRadius.w;
    o.scleraColour    = sp.scleraColour.xyz;
    o.tapetumColour   = sp.tapetumColourAndPresence.xyz;
    o.tapetumPresence = sp.tapetumColourAndPresence.w;

    return o;
}

// ==============================================================================
// == G-Buffer Structs ==========================================================
// ==============================================================================

struct GeometrySample {
    vec3  pos;
    vec3  normal;    // world-space, after all normal map / blend / seam work
    float depth;     // closestT
    int   hitType;   // 0 = body, 1 = left eye, 2 = right eye
};

struct MaterialSample {
    vec3  albedo;
    float specPower;
    float specMask;
    float headSpecPower;
    float headSpecMask;
    float eyelidCoverage;
};

// ==============================================================================
// == Ray Helpers ===============================================================
// ==============================================================================

struct Ray {
    vec3 origin;
    vec3 dir;
};

Ray reconstructRay(vec2 fragCoord)
{
    vec2  ndc         = (fragCoord / u_viewportSize) * 2.0 - 1.0;
    float aspectRatio = u_viewportSize.x / u_viewportSize.y;
    float halfTanFov  = tan(u_fovY * 0.5);

    vec3 dir = normalize(
        u_cameraForward
        + ndc.x * aspectRatio * halfTanFov * u_cameraRight
        + ndc.y * halfTanFov  * u_cameraUp
    );
    dir.z = -dir.z;

    return Ray(u_cameraPos, dir);
}

// ==============================================================================
// == Sphere Intersection =======================================================
// ==============================================================================

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

// ==============================================================================
// == Normal Map ================================================================
// ==============================================================================

vec3 normalFromNormalMap(vec2 uv, float speciesIdx)
{
    vec2 packedNormal   = texture(u_charNormalTex, vec3(uv, speciesIdx)).rg;
    vec3 tangentNormal;
    tangentNormal.xy    = packedNormal * 2.0 - 1.0;
    tangentNormal.z     = sqrt(max(0.0, 1.0 - dot(tangentNormal.xy, tangentNormal.xy)));
    return tangentNormal;
}

// ==============================================================================
// == Atmospheric Adjustments ===================================================
// ==============================================================================

vec3 getDampedColour(vec3 rawColour, float distanceToCam)
{
    float startDampDist   = 50000.0;
    float maxDampDist     = 200000.0;
    float dampFactor      = smoothstep(0.0, 1.0,
                            clamp((distanceToCam - startDampDist) / (maxDampDist - startDampDist), 0.0, 1.0));
    vec3  atmosphericBase = vec3(0.53, 0.58, 0.64);
    return mix(rawColour, atmosphericBase, dampFactor);
}

// ==============================================================================
// == Shadow Test ===============================================================
// ==============================================================================

bool shadowTestSphere(vec3 rayOrigin, vec3 rayDir, vec3 centre, float radius, float maxDist)
{
    vec3  oc           = rayOrigin - centre;
    float b            = 2.0 * dot(oc, rayDir);
    float c            = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * c;

    if (discriminant < 0.0) return false;

    float sqrtD = sqrt(discriminant);
    float t0    = (-b - sqrtD) * 0.5;
    float t1    = (-b + sqrtD) * 0.5;

    if (t0 > 0.001 && t0 < maxDist) return true;
    if (t1 > 0.001 && t1 < maxDist) return true;

    return false;
}

// ==============================================================================
// == PHASE 1 — Geometry ========================================================
// ==============================================================================

GeometrySample resolveGeometry(Ray ray, OrbInstance orb, SphereHit finalHit, int hitType)
{
    GeometrySample geo;
    geo.pos     = finalHit.pos;
    geo.normal  = finalHit.normal;   // will be refined below
    geo.depth   = finalHit.t;
    geo.hitType = hitType;

    vec3 activeNormal = finalHit.normal;

    if (hitType == 0)
    {
        // Body — apply normal map in tangent space
        vec3 localNorm = vec3(
            dot(finalHit.normal, orb.right),
            dot(finalHit.normal, orb.up),
            dot(finalHit.normal, orb.forward)
        );

        float u = (atan(localNorm.z, -localNorm.x) / 3.1415926535) * 0.5 + 0.5;
        float v = acos(clamp(localNorm.y, -1.0, 1.0)) / 3.1415926535;
        vec2  uv = vec2(u, v);

        vec3 tangentNormal = normalFromNormalMap(uv, orb.speciesRaw);
        vec3 N = finalHit.normal;
        vec3 T = normalize(vec3(-localNorm.z, 0.0, localNorm.x));
        T = normalize(T.x * orb.right + T.y * orb.up + T.z * orb.forward);
        vec3 B = cross(N, T);

        activeNormal = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
    }
    else
    {
        // Eye — compute local coordinates relative to the struck eye
        vec3 eyeCentre   = (hitType == 1) ? orb.leftEyeCentre : orb.rightEyeCentre;
        vec3 eyeLocalNorm = normalize(finalHit.pos - eyeCentre);
        float eyeLocalY   = dot(eyeLocalNorm, orb.up);

        activeNormal = eyeLocalNorm;

        // Eyelid closure normal bend
        float eyelidThreshold = mix(-0.6, 1.0, orb.eyelidClosure);
        float lipWidth        = 0.03;
        float eyelidMask      = smoothstep(eyelidThreshold, eyelidThreshold - lipWidth, -eyeLocalY);
        float lipFactor       = 4.0 * eyelidMask * (1.0 - eyelidMask);

        if (lipFactor > 0.0) {
            vec3 lidTiltDir      = orb.up * sign(-eyeLocalY - eyelidThreshold);
            vec3 thicknessNormal = normalize(eyeLocalNorm * 0.3 + lidTiltDir * 0.7);
            activeNormal         = normalize(mix(activeNormal, thicknessNormal, lipFactor * 0.8));
        }
    }

    // Seam blending between eye and body normals
    float blendRadius = orb.radius * 0.05;

    if (hitType == 1 || hitType == 2)
    {
        // On an eye — blend normal toward body near the socket seam,
        // but suppress where the eyelid is already painting over
        vec3  eyeCentre       = (hitType == 1) ? orb.leftEyeCentre : orb.rightEyeCentre;
        vec3  eyeLocalNorm    = normalize(finalHit.pos - eyeCentre);
        float eyeLocalY       = dot(eyeLocalNorm, orb.up);
        float eyelidThreshold = mix(-0.6, 1.0, orb.eyelidClosure);
        float lipWidth        = 0.03;
        float eyelidMask      = smoothstep(eyelidThreshold, eyelidThreshold - lipWidth, -eyeLocalY);

        float distToBodyCenter  = length(finalHit.pos - orb.centre);
        float distToBodySurface = abs(distToBodyCenter - orb.radius);

        if (distToBodySurface < blendRadius) {
            float blendFactor   = smoothstep(blendRadius, 0.0, distToBodySurface);
            blendFactor        *= eyelidMask;
            vec3 bodyNormal     = normalize(finalHit.pos - orb.centre);
            activeNormal        = normalize(mix(activeNormal, bodyNormal, blendFactor * 0.7));
        }
    }
    else // hitType == 0
    {
        // On the body — carve a subtle concave socket where the eyes sit
        float distToLeftEye  = length(finalHit.pos - orb.leftEyeCentre)  - orb.eyeRadius;
        float distToRightEye = length(finalHit.pos - orb.rightEyeCentre) - orb.eyeRadius;
        float minDistToEye   = min(abs(distToLeftEye), abs(distToRightEye));

        if (minDistToEye < blendRadius) {
            float blendFactor    = smoothstep(blendRadius, 0.0, minDistToEye);
            vec3  targetEyeCentre = (abs(distToLeftEye) < abs(distToRightEye))
                                  ? orb.leftEyeCentre : orb.rightEyeCentre;

            vec3  bodyToEye       = normalize(finalHit.pos - targetEyeCentre);
            float eyeLocalY       = dot(bodyToEye, orb.up);
            float eyelidThreshold = mix(-0.6, 1.0, orb.eyelidClosure);
            float lipWidth        = 0.03;
            float lidCoverage     = smoothstep(eyelidThreshold, eyelidThreshold - lipWidth, -eyeLocalY);

            vec3 eyeNormalAtHit = normalize(finalHit.pos - targetEyeCentre);
            vec3 invertedCarve  = normalize(reflect(activeNormal, eyeNormalAtHit));

            activeNormal = normalize(mix(activeNormal, invertedCarve,
                                        blendFactor * (1.0 - lidCoverage) * 0.8));
        }
    }

    geo.normal = activeNormal;
    return geo;
}

// ==============================================================================
// == PHASE 2 — Material ========================================================
// ==============================================================================

MaterialSample resolveMaterial(GeometrySample geo, OrbInstance orb, Ray ray, float lampIntensityMask)
{
    MaterialSample mat;
    mat.eyelidCoverage = 0.0;

    vec3 albedo = orb.scleraColour;

    if (geo.hitType == 0)
    {
        // Inside resolveMaterial (hitType == 0)
        vec3 rawSurfaceNormal = normalize(geo.pos - orb.centre); // Recover pristine surface vector

        vec3 localNorm = vec3(
            dot(rawSurfaceNormal, orb.right),
            dot(rawSurfaceNormal, orb.up),
            dot(rawSurfaceNormal, orb.forward)
        );
        float u = (atan(localNorm.z, -localNorm.x) / 3.1415926535) * 0.5 + 0.5;
        float v = acos(clamp(localNorm.y, -1.0, 1.0)) / 3.1415926535;
        albedo = texture(u_charDiffuseTex, vec3(u, v, orb.speciesRaw)).rgb;

        mat.specPower     = 12.0;
        mat.specMask      = 0.14;
        mat.headSpecPower = 12.0;
        mat.headSpecMask  = 0.04;
    }
    else
    {
        // Eye — resolve pupil / iris / eyelid layers
        vec3  eyeCentre    = (geo.hitType == 1) ? orb.leftEyeCentre : orb.rightEyeCentre;
        vec3  eyeLocalNorm = normalize(geo.pos - eyeCentre);
        float eyeLocalY    = dot(eyeLocalNorm, orb.up);
        float eyeLocalZ    = dot(eyeLocalNorm, orb.gazeDir);

        float pupilRadius  = 0.10 * orb.dilation;
        float irisRadius   = orb.irisRadius;

        if (eyeLocalZ > (1.0 - pupilRadius)) {
            albedo = vec3(0.0);

            if (orb.tapetumPresence > 0.5) {
                float alignment      = max(dot(-normalize(ray.dir), orb.gazeDir), 0.0);
                float retroReflection = pow(alignment, 16.0) * lampIntensityMask;
                albedo              += orb.tapetumColour * retroReflection * 15.0;
            }
        }
        else if (eyeLocalZ > (1.0 - irisRadius)) {
            albedo = orb.irisColour;
        }

        // Eyelid — paint body texture over the eye where the lid has closed
        float eyelidThreshold = mix(-0.6, 1.0, orb.eyelidClosure);
        float lipWidth        = 0.03;
        float eyelidMask      = smoothstep(eyelidThreshold, eyelidThreshold - lipWidth, -eyeLocalY);
        mat.eyelidCoverage    = eyelidMask;

        if (eyelidMask > 0.0) {
            vec3 bodyLocalNorm = normalize(geo.pos - orb.centre);
            vec3 furLocalNorm  = vec3(
                dot(bodyLocalNorm, orb.right),
                dot(bodyLocalNorm, orb.up),
                dot(bodyLocalNorm, orb.forward)
            );
            float furU   = (atan(furLocalNorm.z, -furLocalNorm.x) / 3.1415926535) * 0.5 + 0.5;
            float furV   = acos(clamp(furLocalNorm.y, -1.0, 1.0)) / 3.1415926535;
            vec3  furSample = texture(u_charDiffuseTex, vec3(furU, furV, orb.speciesRaw)).rgb;
            albedo       = mix(albedo, furSample, eyelidMask);
        }

        // Blend spec parameters toward body values wherever the eyelid covers the eye
        mat.specPower     = mix(64.0,  12.0, eyelidMask);
        mat.specMask      = mix(0.85,  0.14, eyelidMask);
        mat.headSpecPower = mix(64.0,  12.0, eyelidMask);
        mat.headSpecMask  = mix(0.35,  0.04, eyelidMask);
    }

    mat.albedo = getDampedColour(albedo, geo.depth);
    return mat;
}

// ==============================================================================
// == PHASE 3 — Lighting ========================================================
// ==============================================================================

vec3 resolveLight(
    GeometrySample geo,
    MaterialSample mat,
    OrbInstance    orb,
    float          sunVisibility,
    float          ambient,
    float          lampIntensityMask,
    vec3           lightDir,
    float          spot,
    float          distToCamera)
{
    // Analytical self-shadowing
    float sunShadow      = 1.0;
    vec3  shadowOrigin   = geo.pos + geo.normal * (orb.radius * 0.01);
    float maxShadowDist  = orb.radius * 3.0;

    if (geo.hitType == 0) {
        bool hitLeft  = shadowTestSphere(shadowOrigin, u_sunDir, orb.leftEyeCentre,  orb.eyeRadius, maxShadowDist);
        bool hitRight = shadowTestSphere(shadowOrigin, u_sunDir, orb.rightEyeCentre, orb.eyeRadius, maxShadowDist);
        if (hitLeft || hitRight) sunShadow = 0.0;
    }
    else {
        bool hitBody = shadowTestSphere(shadowOrigin, u_sunDir, orb.centre, orb.radius, maxShadowDist);
        if (hitBody) sunShadow = 0.0;
    }

    vec3 viewDir = normalize(u_cameraPos - geo.pos);

    // Sun diffuse
    float diffuse = max(dot(geo.normal, u_sunDir), 0.0) * sunVisibility * sunShadow;

    // Sun specular (Blinn-Phong)
    vec3  sunHalf  = normalize(u_sunDir + viewDir);
    float sunSpec  = pow(max(dot(geo.normal, sunHalf), 0.0), mat.specPower)
                   * mat.specMask * sunVisibility * sunShadow;

    // Headlamp diffuse
    float headDiff        = max(dot(geo.normal, lightDir), 0.0);
    float headlampDiffuse = headDiff * lampIntensityMask;

    // Headlamp specular
    vec3  headHalf     = normalize(lightDir + viewDir);
    float headlampSpec = pow(max(dot(geo.normal, headHalf), 0.0), mat.headSpecPower)
                       * mat.headSpecMask * lampIntensityMask;

    vec3 headlampContribution = vec3(0.0);
    if (distToCamera > 0.1 && spot > 0.001) {
        vec3 lampColour        = vec3(1.0, 0.95, 0.85);
        headlampContribution  = mat.albedo * headlampDiffuse * lampColour
                              + vec3(headlampSpec) * lampColour;
    }

    vec3 sunSpecColour = u_sunColour.xyz * sunSpec;
    return mat.albedo * (ambient + (1.0 - ambient) * diffuse)
         + sunSpecColour
         + headlampContribution;
}

// ==============================================================================
// == Main ======================================================================
// ==============================================================================

void main()
{
    // SFML Y-flip
    vec2 fragSFML    = vec2(gl_FragCoord.x, u_viewportSize.y - gl_FragCoord.y);

    Ray         ray = reconstructRay(fragSFML);
    OrbInstance orb = unpackOrb(v_instanceID);

    // Intersect ray with body and both eyes
    SphereHit bodyHit  = intersectSphere(ray, orb.centre,         orb.radius);
    SphereHit leftHit  = intersectSphere(ray, orb.leftEyeCentre,  orb.eyeRadius);
    SphereHit rightHit = intersectSphere(ray, orb.rightEyeCentre, orb.eyeRadius);

    if (!bodyHit.hit && !leftHit.hit && !rightHit.hit) discard;

    // Select the closest surface
    float    closestT = 1e10;
    SphereHit finalHit;
    int      hitType = 0;

    if (bodyHit.hit  && bodyHit.t  < closestT) { closestT = bodyHit.t;  finalHit = bodyHit;  hitType = 0; }
    if (leftHit.hit  && leftHit.t  < closestT) { closestT = leftHit.t;  finalHit = leftHit;  hitType = 1; }
    if (rightHit.hit && rightHit.t < closestT) { closestT = rightHit.t; finalHit = rightHit; hitType = 2; }

    // Time of day factors
    float sunElevationDeg = asin(clamp(u_sunDir.y, -1.0, 1.0)) * 180.0 / 3.1415926535;
    float sunVisibility   = smoothstep(-12.0, 6.0, sunElevationDeg);
    float nightFactor     = smoothstep(-5.0, -15.0, sunElevationDeg);
    float ambient         = mix(0.012, 0.33, sunVisibility);

    // Headlamp
    vec3  toFragment    = finalHit.pos - u_cameraPos;
    float distToCamera  = length(toFragment);
    vec3  toFragDir     = normalize(toFragment);

    vec3  raySpaceForward  = vec3(u_cameraForward.x, u_cameraForward.y, -u_cameraForward.z);
    float spotCos          = dot(raySpaceForward, toFragDir);
    float spot             = pow(max(spotCos, 0.0), 48.0) + pow(max(spotCos, 0.0), 6.0) * 0.08;
    float distFalloff      = pow(max(0.0, 1.0 - distToCamera / u_headlampRange), 1.6);
    float nearFade         = smoothstep(0.0, 1.0, distToCamera);
    float lampIntensityMask = spot * distFalloff * nearFade * u_headlampIntensity * u_headlampEnabled;
    vec3  lightDir         = -toFragDir;

    // Three-phase evaluation
    GeometrySample geo = resolveGeometry(ray, orb, finalHit, hitType);
    MaterialSample mat = resolveMaterial(geo, orb, ray, lampIntensityMask);
    vec3 finalColour    = resolveLight(geo, mat, orb,
                                      sunVisibility, ambient,
                                      lampIntensityMask, lightDir,
                                      spot, distToCamera);

    fragColour = vec4(finalColour, 1.0);
}