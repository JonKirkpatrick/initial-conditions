#version 460 core

// fenestra_demo.frag
// Demo fragment shader for Fenestra.

uniform sampler2D u_wolfTex;
uniform sampler2D u_wolfHeightTex;

// Camera uniforms
uniform vec2  u_viewportSize;
uniform float u_fovY;
uniform vec3  u_cameraPos;
uniform vec3  u_cameraRight;
uniform vec3  u_cameraUp;
uniform vec3  u_cameraForward;

// Per-orb uniforms — hardcoded for demo, will come from SSBO later
uniform vec3  u_orbCentre;
uniform float u_orbRadius;
uniform vec3  u_orbForward;
uniform vec3  u_orbRight;
uniform vec3  u_orbUp;

// Sun for basic shading
uniform vec3  u_sunDir;       // world-space, normalised

in vec4 v_color;              // orb index — unused in demo but present for pipeline compatibility
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

vec3 normalFromHeightMap(vec2 uv, float strength)
{
    // Sample neighbouring texels to compute gradient
    vec2 texelSize = vec2(1.0) / vec2(textureSize(u_wolfHeightTex, 0));
    
    float hL = texture(u_wolfHeightTex, uv + vec2(-texelSize.x, 0.0)).r;
    float hR = texture(u_wolfHeightTex, uv + vec2( texelSize.x, 0.0)).r;
    float hD = texture(u_wolfHeightTex, uv + vec2(0.0, -texelSize.y)).r;
    float hU = texture(u_wolfHeightTex, uv + vec2(0.0,  texelSize.y)).r;

    // Finite difference gradient
    vec3 tangentNormal = normalize(vec3(
        (hL - hR) * strength,
        (hD - hU) * strength,
        1.0
    ));
    
    return tangentNormal;
}

// == Main ======================================================================

void main()
{
    // SFML Y-flip
    vec2 fragSFML = vec2(gl_FragCoord.x, u_viewportSize.y - gl_FragCoord.y);

    Ray ray = reconstructRay(fragSFML);

    // --- Eyeball Placement Geometry ---
    float eyeRadius  = u_orbRadius * 0.22; // Scale eyeballs relative to body size
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

    // === Shading Pass ===
    vec3 sunDir   = normalize(u_sunDir);
    float diffuse = max(dot(finalHit.normal, sunDir), 0.0);
    float ambient = 0.15;
    
    vec3 baseColor = vec3(1.0);

if (hitType == 0) {
    vec3 localNorm = vec3(
        dot(finalHit.normal, normalize(u_orbRight)),
        dot(finalHit.normal, normalize(u_orbUp)),
        dot(finalHit.normal, normalize(u_orbForward))
    );
    float u = (atan(localNorm.z, -localNorm.x) / 3.1415926535) * 0.5 + 0.5;
    float v = acos(clamp(localNorm.y, -1.0, 1.0)) / 3.1415926535;
    vec2 uv = vec2(u, v);

    // Eye positions in local space — starting from your values
    float eyeForward = 1.00;
    float eyeSide    = 0.48;   // try 0.37–0.40 if eyes feel too close together
    float eyeUp      = 0.40;

    vec3 leftEyeDir  = normalize(vec3( eyeSide, eyeUp, eyeForward));
    vec3 rightEyeDir = normalize(vec3(-eyeSide, eyeUp, eyeForward));

    float leftEyeU  = (atan(leftEyeDir.z, -leftEyeDir.x) / 3.1415926535) * 0.5 + 0.5;
    float leftEyeV  = acos(clamp(leftEyeDir.y, -1.0, 1.0)) / 3.1415926535;
    float rightEyeU = (atan(rightEyeDir.z, -rightEyeDir.x) / 3.1415926535) * 0.5 + 0.5;
    float rightEyeV = acos(clamp(rightEyeDir.y, -1.0, 1.0)) / 3.1415926535;

    vec2 leftUV  = vec2(leftEyeU, leftEyeV);
    vec2 rightUV = vec2(rightEyeU, rightEyeV);

    // Distances
    float distL = length(uv - leftUV);
    float distR = length(uv - rightUV);

    // Tighter, separate sockets
    float maskL = smoothstep(0.0, 0.08, distL);   // tighter than 0.12
    float maskR = smoothstep(0.0, 0.08, distR);
    float eyeMask = min(maskL, maskR);

    // === Production look (remove harsh green) ===
    vec3 furColor = texture(u_wolfTex, uv).rgb;
    // Softer darkening + thinning with quadratic falloff
    baseColor = mix(vec3(0.20, 0.15, 0.11), furColor, eyeMask * eyeMask);

    // === Socket depression ===
    float socketDepth = max(
        1.0 - smoothstep(0.0, 0.13, distL),
        1.0 - smoothstep(0.0, 0.13, distR)
    );

    vec3 tangentNormal = normalFromHeightMap(uv, 5.0);
    tangentNormal.z += socketDepth * -0.32;   // slightly softer depth
    tangentNormal = normalize(tangentNormal);

    vec3 T = normalize(u_orbRight);
    vec3 B = normalize(u_orbUp);
    vec3 N = normalize(finalHit.normal);
    vec3 worldNormal = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);

    diffuse = max(dot(worldNormal, sunDir), 0.0);
}
    else {
        // We hit an eyeball! Paint it black (or design a pupil using the eye's local normal)
        vec3 eyeCentre = (hitType == 1) ? leftEyeCentre : rightEyeCentre;
        
        // Find the local normal of the eyeball itself
        vec3 eyeLocalNorm = normalize(finalHit.pos - eyeCentre);
        // How far from the eye centre are we, normalised [0,1]
        float eyeEdge = length(eyeLocalNorm.xy) / 1.0;  // 1.0 at the silhouette

        // Sample fur texture at the same UV the body would have at this point
        // Use the world position of the eye fragment projected onto the body's UV space
        vec3 eyeSurfaceWorld = finalHit.pos;
        vec3 bodyLocalNorm = normalize(eyeSurfaceWorld - u_orbCentre);
        vec3 furLocalNorm = vec3(
            dot(bodyLocalNorm, normalize(u_orbRight)),
            dot(bodyLocalNorm, normalize(u_orbUp)),
            dot(bodyLocalNorm, normalize(u_orbForward))
        );
        float furU = (atan(furLocalNorm.z, -furLocalNorm.x) / 3.1415926535) * 0.5 + 0.5;
        float furV = acos(clamp(furLocalNorm.y, -1.0, 1.0)) / 3.1415926535;
        vec3 furSample = texture(u_wolfTex, vec2(furU, furV)).rgb;

        // Blend fur onto the eye edge
        float furBlend = smoothstep(0.6, 1.0, eyeEdge);
        baseColor = mix(baseColor, furSample, furBlend);
        
        // Create a basic forward-facing pupil on the eye surface
        float pupilDot = dot(eyeLocalNorm, u_orbForward);
        if (pupilDot > 0.95) {
            baseColor = vec3(0.0); // Black pupil
        }
    }

    // Apply lighting uniformly across whatever surface won the depth calculation
    vec3 finalColor = baseColor * (ambient + (1.0 - ambient) * diffuse);
    fragColor = vec4(finalColor, 1.0);
}