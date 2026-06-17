#version 460 core

// fenestra_demo.frag
// Demo fragment shader for Fenestra.
// Renders a single white sphere with three orientation dots:
//   Red   — forward direction
//   Green — up direction  
//   Blue  — right direction
// Verify: sphere should look correctly round with perspective,
// dots should sit on the surface and rotate correctly with the orb.

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
    // Convert to NDC [-1, 1]
    vec2 ndc = (fragCoord / u_viewportSize) * 2.0 - 1.0;

    // Account for aspect ratio and field of view
    float aspectRatio = u_viewportSize.x / u_viewportSize.y;
    float halfTanFov  = tan(u_fovY * 0.5);

    // Build ray direction in world space from camera basis vectors
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

// == Dot marker helper =========================================================

// Returns 1.0 if the surface normal is within angularRadius of the target
// direction, 0.0 otherwise. Used to paint orientation dots on the sphere.
float orientationDot(vec3 normal, vec3 targetDir, float angularRadius)
{
    float cosAngle = dot(normalize(normal), normalize(targetDir));
    return smoothstep(cos(angularRadius), cos(angularRadius * 0.5), cosAngle);
}

// == Main ======================================================================

void main()
{
    // SFML Y-flip
    vec2 fragSFML = vec2(gl_FragCoord.x, u_viewportSize.y - gl_FragCoord.y);

    Ray ray = reconstructRay(fragSFML);

    // --- Eyeball Placement Geometry ---
    float eyeRadius  = u_orbRadius * 0.22; // Scale eyeballs relative to body size
    float forwardPush = u_orbRadius * 0.85; // Push eyes toward the front face
    float sideSpread  = u_orbRadius * 0.45; // Spread eyes out to the left/right sides
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
        // We hit the main body: paint it white (or look up planet skins)
        baseColor = vec3(1.0);
        
        // Keep your debug orientation dots painted strictly on the body shell
        float dotRadius = radians(15.0);
        float fwdDot   = orientationDot(finalHit.normal, u_orbForward, dotRadius);
        float rightDot = orientationDot(finalHit.normal, u_orbRight,   dotRadius);
        float upDot    = orientationDot(finalHit.normal, u_orbUp,      dotRadius);

        baseColor = mix(baseColor, vec3(0.0, 0.0, 1.0), upDot);
        baseColor = mix(baseColor, vec3(0.0, 1.0, 0.0), rightDot);
        baseColor = mix(baseColor, vec3(1.0, 0.0, 0.0), fwdDot);
    } 
    else {
        // We hit an eyeball! Paint it black (or design a pupil using the eye's local normal)
        vec3 eyeCentre = (hitType == 1) ? leftEyeCentre : rightEyeCentre;
        
        // Find the local normal of the eyeball itself
        vec3 eyeLocalNorm = normalize(finalHit.pos - eyeCentre);
        
        // Create a basic forward-facing pupil on the eye surface
        float pupilDot = dot(eyeLocalNorm, u_orbForward);
        if (pupilDot > 0.85) {
            baseColor = vec3(0.0); // Black pupil
        } else {
            baseColor = vec3(0.9, 0.9, 0.85); // Creamy white sclera
        }
    }

    // Apply lighting uniformly across whatever surface won the depth calculation
    vec3 finalColor = baseColor * (ambient + (1.0 - ambient) * diffuse);
    fragColor = vec4(finalColor, 1.0);
}