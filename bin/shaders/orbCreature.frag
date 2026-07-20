#version 460 core

#include "common/raySphere.glsl"
#include "orb/orbData.glsl"
#include "orb/orbSpecies.glsl"
#include "orb/orbCompound.glsl"
#include "ubos/camera.glsl"

// ==============================================================================
// == Varyings ===================================================================
// ==============================================================================
flat in int v_instanceID;

// ==============================================================================
// == Remaining Uniforms ========================================================
// ==============================================================================
uniform sampler2DArray u_charDiffuseTex;
uniform sampler2DArray u_charNormalTex;

// G-Buffer Slots
layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outIndices;
layout (location = 3) out vec4 outRetro;

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
};

OrbInstance unpackOrb(int instanceID)
{
    OrbData raw = orbs[instanceID];
    OrbInstance o;

    o.centre            = raw.centreAndSpeciesIdx.xyz;
    o.speciesRaw        = raw.centreAndSpeciesIdx.w;
    o.forward           = raw.forwardAndRadius.xyz;
    o.radius            = raw.forwardAndRadius.w;
    o.dilation          = raw.gazeDirDilationAndEyelidClosure.z;
    o.right             = raw.rightPadded.xyz;
    o.eyelidClosure     = raw.gazeDirDilationAndEyelidClosure.w;
    o.up                = raw.upPadded.xyz;

    // Decode gaze direction from compact 2D representation
    float maxGazeSpread = 0.57735027;
    vec2  gazeDirRaw    = raw.gazeDirDilationAndEyelidClosure.xy;
    vec3  gazeTarget    = o.forward
                        + (gazeDirRaw.x * maxGazeSpread * o.right)
                        + (gazeDirRaw.y * maxGazeSpread * o.up);
    o.gazeDir = normalize(gazeTarget);

    // Derive eye placement geometry (shared with the shadow pass via orbCompound.glsl)
    OrbEyeGeometry eyes = deriveOrbEyeGeometry(o.centre, o.forward, o.right, o.up, o.radius);
    o.leftEyeCentre  = eyes.leftEyeCentre;
    o.rightEyeCentre = eyes.rightEyeCentre;
    o.eyeRadius      = eyes.eyeRadius;

    // Pull species data
    int speciesIdx = int(o.speciesRaw);
    SpeciesData sp = species[speciesIdx];

    o.irisColour      = sp.irisColourAndRadius.xyz;
    o.irisRadius      = sp.irisColourAndRadius.w;
    o.scleraColour    = sp.scleraColour.xyz;

    return o;
}

// ==============================================================================
// == Per-Pass Structs ==========================================================
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

    float pupilMask;   // 1.0 inside the pupil disc, 0.0 elsewhere
    float speciesIdx;  // pass-through so lighting can re-index SpeciesBuffer
};

// ==============================================================================
// == Camera Ray ================================================================
// ==============================================================================

Ray reconstructRay(vec2 fragCoord)
{
    vec2  ndc         = (fragCoord / u_viewportSize) * 2.0 - 1.0;
    float aspectRatio = u_viewportSize.x / u_viewportSize.y;
    float halfTanFov  = tan(u_fovY * 0.5);

    vec3 viewDir = normalize(
        u_cameraForward 
        + (ndc.x * aspectRatio * halfTanFov * u_cameraRight) 
        + (ndc.y * halfTanFov  * u_cameraUp)
    );

    return Ray(u_cameraPos, viewDir);
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
// == Buffer Packing Helpers ====================================================
// ==============================================================================

vec2 packOctahedral(vec3 v) {
    v /= (abs(v.x) + abs(v.y) + abs(v.z));
    vec2 signV = vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
    return v.z >= 0.0 ? v.xy : (1.0 - abs(v.yx)) * signV;
}

// ==============================================================================
// == PHASE 1 — Geometry ========================================================
// ==============================================================================

GeometrySample resolveGeometry(Ray ray, OrbInstance orb, SphereHit finalHit, int hitType)
{
    GeometrySample geo;
    geo.pos     = finalHit.pos;
    geo.normal  = finalHit.normal;
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
        outIndices.x = 0.0;
    }
    else
    {
        vec3 eyeCentre   = (hitType == 1) ? orb.leftEyeCentre : orb.rightEyeCentre;
        vec3 eyeLocalNorm = normalize(finalHit.pos - eyeCentre);
        float eyeLocalY   = dot(eyeLocalNorm, orb.up);

        activeNormal = eyeLocalNorm;

        float eyelidThreshold = mix(-0.6, 1.0, orb.eyelidClosure);
        float lipWidth        = 0.03;
        float eyelidMask      = smoothstep(eyelidThreshold, eyelidThreshold - lipWidth, -eyeLocalY);
        float lipFactor       = 4.0 * eyelidMask * (1.0 - eyelidMask);
        outIndices.x = 1.0/255;

        if (lipFactor > 0.0) {
            vec3 lidTiltDir      = orb.up * sign(-eyeLocalY - eyelidThreshold);
            vec3 thicknessNormal = normalize(eyeLocalNorm * 0.3 + lidTiltDir * 0.7);
            activeNormal         = normalize(mix(activeNormal, thicknessNormal, lipFactor * 0.8));
            outIndices.x = 0.0;
        }
    }

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

MaterialSample resolveMaterial(GeometrySample geo, OrbInstance orb)
{
    MaterialSample mat;
    mat.eyelidCoverage = 0.0;
    mat.pupilMask       = 0.0;
    mat.speciesIdx      = orb.speciesRaw / 255.0;

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
        vec3  eyeCentre    = (geo.hitType == 1) ? orb.leftEyeCentre : orb.rightEyeCentre;
        vec3  eyeLocalNorm = normalize(geo.pos - eyeCentre);
        float eyeLocalY    = dot(eyeLocalNorm, orb.up);
        float eyeLocalZ    = dot(eyeLocalNorm, orb.gazeDir);

        float pupilRadius  = 0.10 * orb.dilation;
        float irisRadius   = orb.irisRadius;

        if (eyeLocalZ > (1.0 - pupilRadius)) {
            // Pure material colour — black pupil. Any retroreflective glow
            // is a function of light/view alignment, so it's added later
            // in resolveLight(), keyed off mat.pupilMask + mat.speciesIdx.
            albedo        = vec3(0.0);
            mat.pupilMask = 1.0;
        }
        else if (eyeLocalZ > (1.0 - irisRadius)) {
            albedo = orb.irisColour;
        }

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

        mat.specPower     = mix(64.0,  12.0, eyelidMask);
        mat.specMask      = mix(0.85,  0.14, eyelidMask);
        mat.headSpecPower = mix(64.0,  12.0, eyelidMask);
        mat.headSpecMask  = mix(0.35,  0.04, eyelidMask);
    }

    mat.albedo = albedo;
    return mat;
}

// ==============================================================================
// == Main ======================================================================
// ==============================================================================

void main()
{
    Ray         ray = reconstructRay(gl_FragCoord.xy);
    OrbInstance orb = unpackOrb(v_instanceID);

    OrbEyeGeometry eyes     = OrbEyeGeometry(orb.leftEyeCentre, orb.rightEyeCentre, orb.eyeRadius);
    OrbCompoundHit compound = intersectOrbCompound(ray, orb.centre, orb.radius, eyes);

    if (!compound.hit.hit) discard;

    GeometrySample geo  = resolveGeometry(ray, orb, compound.hit, compound.hitType);
    MaterialSample mat  = resolveMaterial(geo, orb);

    // Since this is a ray cast sphere and not a proper mesh, the depth buffer write needs to be explicit.
    vec4 clipPos        = u_viewProj * vec4(geo.pos, 1.0);
    float ndcZ          = clipPos.z / clipPos.w;
    gl_FragDepth        = ndcZ * 0.5 + 0.5;

    outAlbedo           = vec4(mat.albedo, 1.0);
    outNormal           = vec4(geo.normal, 1.0);
    outIndices.y        = float(mat.speciesIdx);
    outIndices.zw       = vec2(1.0, 1.0);

    float actualPupilMask = 0.0;
    if (geo.hitType == 1 || geo.hitType == 2) { // It's an eye
        if (mat.pupilMask > 0.5 && mat.eyelidCoverage < 1.0) {
            actualPupilMask = 1.0;
        }
    }

    vec2 encodedGaze = packOctahedral(orb.gazeDir) * 0.5 + 0.5;
    outRetro = vec4(encodedGaze, actualPupilMask, 1.0);
}
