#version 460 core

// Demo fragment shader for Fenestra.

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

// Texture samples
uniform sampler2D u_charTex;
uniform sampler2D u_charNormalTex;
uniform sampler2D u_bakeTex;
uniform sampler2D u_heightMap;

// World bounds
uniform vec2        u_topdownWorldMin;
uniform vec2        u_topdownWorldSize;
uniform float       u_topdownHeightMax;

// Camera uniforms
uniform vec2  u_viewportSize;
uniform float u_fovY;
uniform vec3  u_cameraPos;
uniform vec3  u_cameraRight;
uniform vec3  u_cameraUp;
uniform vec3  u_cameraForward;

// Light uniforms
uniform vec3  u_sunDir;       // world-space, normalised
uniform vec3  u_sunColor;
uniform float u_headlampIntensity;
uniform float u_headlampRange;
uniform float u_headlampCone; // cos(angle) of headlamp cone for cutoff
uniform float u_headlampEnabled;

OrbData orb = orbs[0];

out vec4 fragColor;

// == Ray helpers ==============================================================

struct Ray {
    vec3 origin;
    vec3 dir;
};

// Reconstruct a world-space ray for this fragment from camera parameters.
Ray reconstructRay(vec2 fragCoord)
{
    vec2 ndc = (fragCoord / u_viewportSize) * 2.0 - 1.0;
    float aspectRatio = u_viewportSize.x / u_viewportSize.y;
    float halfTanFov  = tan(u_fovY * 0.5);

    // Note: u_cameraForward is negated on upload (see renderDemoSphere).
    // dir.z is flipped here to reconcile SFML/engine coordinate conventions
    // with OpenGL's ray casting space. Both corrections are intentional.
    vec3 dir = normalize(
        u_cameraForward
        + ndc.x * aspectRatio * halfTanFov * u_cameraRight
        + ndc.y * halfTanFov  * u_cameraUp
    );
    dir.z = -dir.z;

    return Ray(u_cameraPos, dir);
}

// == Sphere intersection =======================================================

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

    vec3  oc = ray.origin - centre;
    float a  = dot(ray.dir, ray.dir);
    float b  = 2.0 * dot(oc, ray.dir);
    float c  = dot(oc, oc) - radius * radius;
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

vec3 normalFromNormalMap(vec2 uv)
{
    vec2 packedNormal = texture(u_charNormalTex, uv).rg;
    vec3 tangentNormal;
    tangentNormal.xy = packedNormal * 2.0 - 1.0;
    tangentNormal.z = sqrt(max(0.0, 1.0 - dot(tangentNormal.xy, tangentNormal.xy)));
    return tangentNormal;
}

// == World Space Reconstruction ================================================

vec2 decodeXZ(vec4 c) {
    return c.xy;
}

float rawHeightAt(ivec2 uv) {
    vec4 c = texelFetch(u_heightMap, uv, 0);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    float scaled = dot(bytes, vec3(65536.0, 256.0, 1.0));
    return scaled * (u_topdownHeightMax / 16777215.0);
}

float decodeHeight(vec2 xz) {
    vec2 uv = (xz - u_topdownWorldMin) / u_topdownWorldSize;
    uv.y = 1.0 - uv.y;
    
    vec2 texSize = vec2(textureSize(u_heightMap, 0));
    vec2 px = uv * (texSize - 1.0);
    
    ivec2 p0 = ivec2(floor(px));
    vec2 f = fract(px);
    
    float h00 = rawHeightAt(p0);
    float h10 = rawHeightAt(p0 + ivec2(1, 0));
    float h01 = rawHeightAt(p0 + ivec2(0, 1));
    float h11 = rawHeightAt(p0 + ivec2(1, 1));
    
    float h0 = mix(h00, h10, f.x);
    float h1 = mix(h01, h11, f.x);
    return mix(h0, h1, f.y);
}

// == Atmospheric Adjustments ===================================================

vec3 getDampedNormal(vec3 rawNormal, float distanceToCam) {
    // Distance boundaries defined in centimeters (1.0 = 1cm)
    // Detail begins softening smoothly at 0.5 km and hits perfectly upright at 2.0 km
    float startDampDist = 50000.0; 
    float maxDampDist   = 200000.0; 
    
    // Compute our blending factor [0.0 = full detail, 1.0 = vertical flat]
    float dampFactor = clamp((distanceToCam - startDampDist) / (maxDampDist - startDampDist), 0.0, 1.0);
    
    // Smooth out the blending rate so the transition at 1.5km isn't a harsh line
    dampFactor = smoothstep(0.0, 1.0, dampFactor);
    
    // Blend smoothly from the computed slope vector straight up toward vec3(0.0, 1.0, 0.0)
    vec3 straightUp = vec3(0.0, 1.0, 0.0);
    return normalize(mix(rawNormal, straightUp, dampFactor));
}

vec3 getDampedColor(vec3 rawColor, float distanceToCam) {
    float startDampDist = 50000.0;  // 0.5 km
    float maxDampDist   = 200000.0; // 2.0 km
    
    float dampFactor = clamp((distanceToCam - startDampDist) / (maxDampDist - startDampDist), 0.0, 1.0);
    dampFactor = smoothstep(0.0, 1.0, dampFactor);
    
    // Artistic Target Color: A desaturated, slightly atmospheric grey-blue hue.
    // This forms a perfect bridge color before the final sky fog layer sweeps over it.
    vec3 atmosphericBase = vec3(0.53, 0.58, 0.64); 
    
    return mix(rawColor, atmosphericBase, dampFactor);
}

// == Main ======================================================================

void main()
{
    // SFML Y-flip
    vec2 fragSFML = vec2(gl_FragCoord.x, u_viewportSize.y - gl_FragCoord.y);
    vec2 fragScreenUv = fragSFML / u_viewportSize;
    vec4 bakeC = texture(u_bakeTex, fragScreenUv);

    Ray ray = reconstructRay(fragSFML);

    // Unpack Uniforms

    vec3 u_orbCentre            = orb.centreAndRadius.xyz;
    float u_orbRadius           = orb.centreAndRadius.w;
    vec3 u_orbForward           = orb.forwardAndDilation.xyz;
    float u_orbDilation         = orb.forwardAndDilation.w;
    vec3 u_orbRight             = orb.rightAndEyelidClosure.xyz;
    float u_orbEyelidClosure    = orb.rightAndEyelidClosure.w;
    vec3 u_tapetumColor         = orb.tapetumColourAndPresence.xyz;
    float u_tapetumPresence     = orb.tapetumColourAndPresence.w;
    vec3 u_orbUp                = orb.upPadded.xyz;
    vec2 gazeDirRaw = orb.gazeDirPadded.xy;
    float maxGazeSpread = 0.57735027;
    vec3 gazeTarget = u_orbForward
                    + (gazeDirRaw.x * maxGazeSpread * u_orbRight)
                    + (gazeDirRaw.y * maxGazeSpread * u_orbUp);
    
    vec3 u_gazeDir              = normalize(gazeTarget);
    vec3 u_squashDirection      = orb.squashAndDirection.xyz;
    float u_squashAmount        = orb.squashAndDirection.w;
    vec3 u_irisColour           = orb.irisAndSpeciesIdx.xyz;
    float speciesRaw            = orb.irisAndSpeciesIdx.w; // Note to self, this will need to be cast as an Int

    // --- Eyeball Placement Geometry ---
    float eyeRadius  =  u_orbRadius * 0.22; // Scale eyeballs relative to body size
    float forwardPush = u_orbRadius * 0.78; // Push eyes toward the front face
    float sideSpread  = u_orbRadius * 0.35; // Spread eyes out to the left/right sides
    float verticalUp  = u_orbRadius * 0.35; // Elevate eyes slightly up from center

    // Combine basis vectors to find true 3D world space centers for both eyes
    vec3 leftEyeCentre  = u_orbCentre 
                        + u_orbForward * forwardPush 
                        + u_orbRight   * sideSpread 
                        + u_orbUp      * verticalUp;

    vec3 rightEyeCentre = u_orbCentre 
                        + u_orbForward * forwardPush 
                        - u_orbRight   * sideSpread 
                        + u_orbUp      * verticalUp;

    // Intersect the single camera ray with all three physical structures
    SphereHit bodyHit  = intersectSphere(ray, u_orbCentre, u_orbRadius);
    SphereHit leftHit  = intersectSphere(ray, leftEyeCentre, eyeRadius);
    SphereHit rightHit = intersectSphere(ray, rightEyeCentre, eyeRadius);

    // If the ray completely misses all three shapes, get rid of the fragment
    if (!bodyHit.hit && !leftHit.hit && !rightHit.hit) {
        discard;
    }

    // Determine which surface is closest to the camera lens (Z-occlusion)
    float closestT = 1e10;
    SphereHit finalHit;
    int hitType = 0; // 0 = Body, 1 = Left Eye, 2 = Right Eye

    if (bodyHit.hit && bodyHit.t < closestT) {
        closestT = bodyHit.t;
        finalHit = bodyHit;
        hitType  = 0;
    }
    if (leftHit.hit && leftHit.t < closestT) {
        closestT = leftHit.t;
        finalHit = leftHit;
        hitType  = 1;
    }
    if (rightHit.hit && rightHit.t < closestT) {
        closestT = rightHit.t;
        finalHit = rightHit;
        hitType  = 2;
    }

    // === Occlude Against Terrain =====

    vec2 landXZ = bakeC.rg;
    vec3 landPos;
    float landDist;
    if (!(bakeC.a == 0.0 && bakeC.r == 0.0))
    {
        float landY   = decodeHeight(landXZ);
        landPos = vec3(landXZ.x, landY, landXZ.y);
        landDist = length(landPos - u_cameraPos);

        if (closestT > landDist) discard;
    }

    // === Time of Day & Shading Calculations ===

    // 1. Calculate Sun Elevation in Degrees from the Y component
    float sunElevationDeg = asin(clamp(u_sunDir.y, -1.0, 1.0)) * 180.0 / 3.1415926535;

    // 2. Generate smooth environment factors
    float sunVisibility = smoothstep(-12.0, 6.0, sunElevationDeg);
    float nightFactor    = smoothstep(-5.0, -15.0, sunElevationDeg);

    // 3. Replicate your precise old ambient values scaled by sun state
    float ambientDay   = 0.33;
    float ambientNight = 0.012;
    float ambient      = mix(ambientNight, ambientDay, sunVisibility);

    // ==================== HEADLAMP POWER SETUP ====================
    // Calculate the raw strength of the flashlight beam hitting this point in space
    vec3 toFragment   = finalHit.pos - u_cameraPos;
    float distToCamera = length(toFragment);
    vec3 toFragDir     = normalize(toFragment);

    vec3 raySpaceForward = vec3(u_cameraForward.x, u_cameraForward.y, -u_cameraForward.z);
    float spotCos = dot(raySpaceForward, toFragDir);

    float spotTight = pow(max(spotCos, 0.0), 48.0);
    float spotSpill = pow(max(spotCos, 0.0), 6.0) * 0.08;
    float spot      = spotTight + spotSpill;
    
    float distFalloff = pow(max(0.0, 1.0 - distToCamera / u_headlampRange), 1.6);
    float nearFade    = smoothstep(0.0, 1.0, distToCamera);
    
    // This is the total photons arriving at this pixel from your flashlight
    float lampIntensityMask = spot * distFalloff * nearFade * u_headlampIntensity * u_headlampEnabled;
    vec3 lightDir = -toFragDir;
    // ==============================================================

    // Declare active normal and default to the smooth sphere geometry
    vec3 activeNormal = finalHit.normal;

    // === Shading Pass ===

    vec3 baseColor = vec3(1.0);

    if (hitType == 0) {
        vec3 localNorm = vec3(
            dot(finalHit.normal, u_orbRight),
            dot(finalHit.normal, u_orbUp),
            dot(finalHit.normal, u_orbForward)
        );
        float u = (atan(localNorm.z, -localNorm.x) / 3.1415926535) * 0.5 + 0.5;
        float v = acos(clamp(localNorm.y, -1.0, 1.0)) / 3.1415926535;
        vec2 uv = vec2(u, v);


        vec3 furColor = texture(u_charTex, uv).rgb;
        baseColor = furColor;

        vec3 tangentNormal = normalFromNormalMap(uv);
        vec3 N = finalHit.normal;
        vec3 T = normalize(vec3(-localNorm.z, 0.0, localNorm.x)); 
        T = normalize(T.x * u_orbRight + T.y * u_orbUp + T.z * u_orbForward);
        vec3 B = cross(N, T); 
        
        // Update active normal with your heightmap data
        activeNormal = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
    }
    else {
        vec3 eyeCentre = (hitType == 1) ? leftEyeCentre : rightEyeCentre;
        
        // Find the local normal of the eyeball itself
        vec3 eyeLocalNorm = normalize(finalHit.pos - eyeCentre);

        // Project into the eye's local coordinate system
        float eyeLocalX = dot(eyeLocalNorm, u_orbRight);
        float eyeLocalY = dot(eyeLocalNorm, u_orbUp);
        float eyeLocalZ = dot(eyeLocalNorm, u_gazeDir);

        // Project the 3D local normal onto the eyeball's local right and up axes
        float eyeEdgeX = dot(eyeLocalNorm, u_orbRight);
        float eyeEdgeY = dot(eyeLocalNorm, u_orbUp);

        float pupilRadius = 0.10 * u_orbDilation;
        float irisRadius  = 0.25;

        // Sample fur texture at the same UV the body would have at this point
        // Use the world position of the eye fragment projected onto the body's UV space
        vec3 eyeSurfaceWorld = finalHit.pos;
        vec3 bodyLocalNorm = normalize(eyeSurfaceWorld - u_orbCentre);
        vec3 furLocalNorm = vec3(
            dot(bodyLocalNorm, u_orbRight),
            dot(bodyLocalNorm, u_orbUp),
            dot(bodyLocalNorm, u_orbForward)
        );
        float furU = (atan(furLocalNorm.z, -furLocalNorm.x) / 3.1415926535) * 0.5 + 0.5;
        float furV = acos(clamp(furLocalNorm.y, -1.0, 1.0)) / 3.1415926535;
        vec3 furSample = texture(u_charTex, vec2(furU, furV)).rgb;
        
        // Create a basic forward-facing pupil on the eye surface
        if (eyeLocalZ > (1.0 - pupilRadius)) {
            baseColor = vec3(0.0); // Pitch black pupil base
            
            if (u_tapetumPresence > 0.5) {
                // The tapetum doesn't care about surface normals! It only cares about 
                // camera alignment and the total incoming light arriving at the eye.
                float alignment = max(dot(-normalize(ray.dir), u_gazeDir), 0.0);
                float retroReflection = pow(alignment, 16.0) * lampIntensityMask;
                
                baseColor += u_tapetumColor * retroReflection * 5.0; 
            }
        } 
        else if (eyeLocalZ > (1.0 - irisRadius)) {
            baseColor = vec3(0.2, 0.5, 0.8);
        }

        // --- EYELID CLOSURE MASK ---
        // Maps closure [0.0, 1.0] to a vertical threshold descending from 1.0 down to -0.2
        float eyelidThreshold = mix(-0.6, 1.0, u_orbEyelidClosure);
        
        // Smooth the lid edge slightly to prevent raw pixel jaggedness
        float eyelidMask = smoothstep(eyelidThreshold, eyelidThreshold - 0.02, -eyeLocalY);
        
        // Blend the fur texture over the eye surface wherever the eyelid is closed
        baseColor = mix(baseColor, furSample, eyelidMask);

        // --- SQUASH INTO SOCKET EDGE ---
        float eyeEdge = length(vec2(eyeLocalX, eyeLocalY));
        float furBlend = smoothstep(0.6, 1.0, eyeEdge);
        baseColor = mix(baseColor, furSample, furBlend);

        activeNormal = eyeLocalNorm;
    }
    baseColor = getDampedColor(baseColor, closestT);
    // ==================== FINAL LIGHTING ACCUMULATION ====================
    // 1. Sun Diffuse (Uses the bumped activeNormal)
    float diffuse = max(dot(activeNormal, u_sunDir), 0.0) * sunVisibility;

    // 2. Headlamp Diffuse (Uses the bumped activeNormal multiplied by the raw flashlight pool)
    float headDiff = max(dot(activeNormal, lightDir), 0.0);
    float headlampDiffuse = headDiff * lampIntensityMask;

    vec3 headlampContribution = vec3(0.0);
    if (distToCamera > 0.1 && spot > 0.001) {
        headlampContribution = baseColor * headlampDiffuse * vec3(1.0, 0.95, 0.85);
    }

    // Combine everything smoothly at the bottom
    vec3 finalColor = baseColor * (ambient + (1.0 - ambient) * diffuse) + headlampContribution;
    fragColor = vec4(finalColor, 1.0);
}