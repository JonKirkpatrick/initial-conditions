#version 460 core

uniform vec3        cameraPos;
uniform vec2        topdownWorldMin;
uniform vec2        topdownWorldSize;
uniform float       topdownHeightMax;
uniform sampler2D   heightMap;
uniform float       nearPlane;
uniform float       farPlane;

uniform vec3        sunDir;
uniform vec3        sunDirWorld;
uniform vec4        sunColor;
uniform sampler2D   bakeTex;
uniform vec2        u_viewportSize;
uniform float       headlampIntensity;
uniform float       headlampRange;
uniform float       headlampConeCos; // cos(angle) of headlamp cone (for cutoff)
uniform float       headlampEnabled; // 1.0 when headlights are on, 0.0 when off

// === Batching support ===
uniform int         u_batchSize;
uniform vec3        u_orbCenterView[64];
uniform vec4        u_orbColor[64];
uniform float       u_orbDepthNorm[64];   // the normalized depth into the scene
uniform vec2        u_quadOrigin[64];     // top left of orb in screen space
uniform vec2        u_texSize[64];        // diameter in pixels per orb
uniform vec2        u_quadSize[64];
uniform float       u_orbRadiusPx[64];
uniform vec3        u_gazeDir[64];
uniform vec3        u_orbForward[64];
uniform float       u_hasTapetum[64];
uniform vec3        u_tapetumColor[64];
uniform float       u_pupilDilation[64];
uniform float       u_eyelidClosure[64];

in vec4 gl_Color;
out vec4 fragColor;

// ================== XZ DECODE ==================
vec2 decodeXZ(vec4 c) {
    return c.xy;
}

// ================== Helper functions ==================
float rawHeightAt(ivec2 uv) {
    vec4 c = texelFetch(heightMap, uv, 0);
    vec3 bytes = floor(c.rgb * 255.0 + 0.5);
    float scaled = dot(bytes, vec3(65536.0, 256.0, 1.0));
    return scaled * (topdownHeightMax / 16777215.0);
}

float decodeHeight(vec2 xz) {
    vec2 uv = (xz - topdownWorldMin) / topdownWorldSize;
    uv.y = 1.0 - uv.y;
    
    vec2 texSize = vec2(textureSize(heightMap, 0));
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

// ================== Eye helpers ==================

// Rotate a direction vector by 'angle' radians around a given axis (Rodrigues)
vec3 rotateAround(vec3 v, vec3 axis, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}

// Given the orb's forward vector (view space), compute the two eye center
// directions as unit vectors on the sphere surface.
// elevationRad: angle up from forward toward +Y
// separationRad: half-angle left/right around the Y-rotated forward
void eyeDirections(vec3 forward, float elevationRad, float separationRad,
                   out vec3 leftEye, out vec3 rightEye)
{
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(forward, up));

    vec3 elevated = normalize(rotateAround(forward, right, elevationRad));
    leftEye  = normalize(rotateAround(elevated, up,  separationRad));
    rightEye = normalize(rotateAround(elevated, up, -separationRad));
}

// Returns > 0 if the fragment at billboard pos 'fragPos' (the same pos you
// use for body shading, range [-1,1]) hits the eye disc, and the eye surface
// is in front of the body surface at that fragment.
// eyeDir: unit vector from eyeDirections()
// eyeRadius: in the same [-1,1] billboard space
// Returns the eye's local 2D offset (for pupil/highlight math), or signals
// a miss via outHit = false.
float eyeSurfaceZ(vec2 eyeCenter, vec2 fragPos, float eyeRadius, out bool hit) {
    vec2 delta = fragPos - eyeCenter;
    float d2 = dot(delta, delta);
    float r2 = eyeRadius * eyeRadius;
    hit = d2 < r2;
    // z on the eye's local sphere surface, in billboard units
    return sqrt(max(0.0, r2 - d2));
}

void main()
{
    // Find which orb this fragment belongs to
    int orbIndex = int(gl_Color.r * 255.0 + 0.5);

    if (orbIndex >= u_batchSize) {
        discard;
    }
    
    vec2 quadOrigin = u_quadOrigin[orbIndex];
    vec2 texSize    = u_texSize[orbIndex];
    vec2 quadSize   = u_quadSize[orbIndex];
    vec3 orbCenter  = u_orbCenterView[orbIndex];
    float depthNorm = u_orbDepthNorm[orbIndex];
    vec4 orbColor   = u_orbColor[orbIndex];
    float blink     = u_eyelidClosure[orbIndex];

    // Convert gl_FragCoord (OpenGL bottom-left Y-up) to SFML window coords (top-left Y-down)
    vec2 fragScreenSFML = vec2(gl_FragCoord.x, u_viewportSize.y - gl_FragCoord.y);

    // Local position inside the billboard quad (SFML coords)
    vec2 localSFML = fragScreenSFML - quadOrigin;
    vec2 uv = localSFML / quadSize;
    float padding = (quadSize / texSize).x;

    // Per-pixel occlusion using screen position
    vec2 fragScreenUv = fragScreenSFML / u_viewportSize;
    vec4 bakeC = texture(bakeTex, fragScreenUv);
    
    if (!(bakeC.a == 0.0 && bakeC.r == 0.0)) {
        
        vec2 xz = decodeXZ(bakeC);
        
        float terrainH = decodeHeight(xz);
        
        vec3 terrainWorld = vec3(xz.x, terrainH, xz.y);
        vec3 relative = terrainWorld - cameraPos;
        float terrainDist = length(relative);
        
        float terrainNorm = clamp((terrainDist - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);

        
        if (terrainNorm < depthNorm) {
            discard;
        }
    }
    vec3 N  = -normalize(orbCenter);
    vec3 up = (abs(N.y) > 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 R  = normalize(cross(up, N));
    vec3 U  = cross(N, R);

    vec3 orbForwardRaw = u_orbForward[orbIndex];
    vec3 orbForward    = normalize(vec3(dot(orbForwardRaw, R), dot(orbForwardRaw, U), dot(orbForwardRaw, N)));

    // vec3 gazeDirRaw = u_gazeDir[orbIndex];
    vec3 gazeDirRaw = vec3(0.0,0.0,1.0);
    vec3 gazeDir    = normalize(vec3(dot(gazeDirRaw, R), dot(gazeDirRaw, U), dot(gazeDirRaw, N)));

    vec2 pos = (uv - vec2(0.5)) * 2.0 * padding;
    float dist = length(pos);

    // 1. Calculate the eye directions in true 3D View Space first, 
    // using the raw, un-projected forward vector of the entity.
    vec3 leftDir, rightDir;
    eyeDirections(normalize(u_orbForward[orbIndex]), radians(22.5), radians(25.0), leftDir, rightDir);

    // 2. Project these true 3D view-space directions onto your billboard's 
    // actual screen-aligned horizontal (R) and vertical (U) axes.
    // This allows the eyes to roll up/down organically when looking from above/below.
    vec2 leftCenter  = vec2(dot(leftDir, R), -dot(leftDir, U));
    vec2 rightCenter = vec2(dot(rightDir, R), -dot(rightDir, U));

    // 3. Exact 3D distance scaling (your downstream code looking for distToHead and radius scaling)
    float distToHead = length(orbCenter);
    float baseEyeRadius = 1.0 / 4.0;

    float distToLeftEye  = distToHead - (leftDir.z  * baseEyeRadius);
    float distToRightEye = distToHead - (rightDir.z * baseEyeRadius);

    float leftScale  = distToHead / max(0.001, distToLeftEye);
    float rightScale = distToHead / max(0.001, distToRightEye);

    float leftEyeRadius  = baseEyeRadius * leftScale;
    float rightEyeRadius = baseEyeRadius * rightScale;

    // 4. Evaluate hits using your scaled circular radii
    bool leftHit, rightHit;
    float leftZ  = eyeSurfaceZ(leftCenter,  pos, leftEyeRadius,  leftHit);
    float rightZ = eyeSurfaceZ(rightCenter, pos, rightEyeRadius, rightHit);

    if (dist > 1.0 && !leftHit && !rightHit) discard;
    
    bool onSphere = dist <= 1.0;
    float z       = onSphere ? sqrt(1.0 - dist * dist) : 0.0;
    vec3 normal   = onSphere ? normalize(vec3(pos.x, -pos.y, z)) : vec3(0.0, 0.0, 1.0);
    float bodyZ   = z;

    bool hasTapetum   = (u_hasTapetum[orbIndex] < 0.5)? false:true;
    // bool hasTapetum = true;
    vec3  tapetumColor = u_tapetumColor[orbIndex].xyz;
    // vec3 tapetumColor = vec3(1.0,0.0,0.0);

    // Your original calculation for the front-facing body depth at the eye center
    float leftBodyZ  = sqrt(max(0.0, 1.0 - dot(leftCenter,  leftCenter)));
    float rightBodyZ = sqrt(max(0.0, 1.0 - dot(rightCenter, rightCenter)));

    // Flip the sign to negative if the eye direction vector points away from the camera
    if (leftDir.z <= 0.0)  leftBodyZ  = -leftBodyZ;
    if (rightDir.z <= 0.0) rightBodyZ = -rightBodyZ;

    // 1. Establish unified Z depths for sorting (higher Z = closer to camera)
    float currentBodyZ  = onSphere ? bodyZ : -1.0; 
    
    // Now, if an eye is on the back hemisphere, its absolute Z depth (bodyZ + localZ) 
    // will naturally be less than the front-facing currentBodyZ, failing the check 
    // and letting the body occlude it perfectly!
    float currentLeftZ  = leftHit  ? (leftBodyZ  + leftZ)  : -2.0;
    float currentRightZ = rightHit ? (rightBodyZ + rightZ) : -2.0;

    // 2. Conditionally determine who wins the fragment
    bool drawLeftEye  = leftHit  && (currentLeftZ  >= currentBodyZ) && (currentLeftZ  >= currentRightZ);
    bool drawRightEye = rightHit && (currentRightZ >= currentBodyZ) && (currentRightZ >  currentLeftZ);
    
    // Two-source sphere shading: sun + camera headlamp.
    vec3 sunLightDir = normalize(sunDir);
    vec3 localSun = vec3(dot(sunLightDir,R),dot(sunLightDir,U),dot(sunLightDir,N));
    float sunDiffuse = max(dot(normal, localSun), 0.0);

    float hemisphere = clamp(0.20 + 0.80 * z, 0.0, 1.0);
    vec3 sunWorldDir = normalize(sunDirWorld);
    float sunElevationDeg = asin(clamp(sunWorldDir.y, -1.0, 1.0)) * 180.0 / 3.14159265;
    float sunVisibility = smoothstep(-12.0, 6.0, sunElevationDeg);
    float nightFactor = smoothstep(-5.0, -15.0, sunElevationDeg);

    vec3 ambientDay = orbColor.rgb * 0.080;
    vec3 ambientNight = orbColor.rgb * 0.012;
    vec3 ambient = mix(ambientNight, ambientDay, sunVisibility);

    vec3 sunShaded = ambient + orbColor.rgb * hemisphere * (0.10 + 0.90 * sunDiffuse * sunVisibility);
    sunShaded += sunColor.rgb * sunDiffuse * 0.28 * sunVisibility;

    // Headlamp cone: screen center beam, using the orb center in view space.
    vec3 orbCenterDir = normalize(orbCenter);
    float centerAngleCos = dot(orbCenterDir, vec3(0.0, 0.0, -1.0));
    float coneSoftness = 0.18;
    float coneMask = smoothstep(headlampConeCos - coneSoftness,
                                min(1.0, headlampConeCos + coneSoftness),
                                centerAngleCos);
    float centerDist = length(orbCenter);
    float rangeT = clamp(centerDist / max(headlampRange, 0.0001), 0.0, 1.0);
    float rangeMask = 1.0 - smoothstep(0.0, 1.0, rangeT);
    rangeMask = pow(rangeMask, 1.25);
    float nightBoost = mix(0.20, 1.0, nightFactor);
    float lampStrength = headlampEnabled * headlampIntensity * coneMask * rangeMask * nightBoost;
    vec3 lampLightDir = vec3(0.0, 0.0, 1.0);
    float lampDiffuse = max(0.0, dot(normal, lampLightDir));
    vec3 flashlightShaded = orbColor.rgb * (0.15 + 0.85 * lampDiffuse);
    flashlightShaded *= lampStrength;

    // Atmospheric perspective: mirror topo_final's smooth dusk/night rolloff.
    float atmosphereMaxDist = 0.5;
    float distNormalized = clamp(depthNorm / atmosphereMaxDist, 0.0, 1.0);
    float atmosphereStrength = pow(distNormalized, 1.7);

    float dayMix = smoothstep(-10.0, 6.0, sunElevationDeg);
    vec3 shaded = ambient;
    shaded += (sunShaded - ambient) * dayMix;
    shaded += flashlightShaded;

    vec3 nightAtm = vec3(0.008, 0.006, 0.025);
    vec3 atmTint = mix(sunColor.rgb * vec3(0.7, 0.65, 0.8),
                       vec3(0.50, 0.68, 0.95), 0.6);
    float daytimeFactor = clamp(sunWorldDir.y * 1.8, 0.0, 1.0);
    atmTint = mix(vec3(0.28, 0.18, 0.40), atmTint, daytimeFactor);
    atmTint = mix(atmTint, nightAtm, nightFactor * 0.95);

    vec3 finalColor = mix(shaded,
                          mix(shaded, atmTint, 0.72),
                          atmosphereStrength * 0.85);

    float desat = atmosphereStrength * mix(0.38, 0.75, nightFactor);
    finalColor = mix(finalColor,
                     vec3(dot(finalColor, vec3(0.299, 0.587, 0.114))),
                     desat);
    finalColor *= mix(1.0, 0.18, nightFactor);

    // === Eye override ===
    bool inEye = false;
    vec3 eyeNormal = vec3(0.0);
    vec2 eyeLocalPos = vec2(0.0);
    float eyeCenterX = 0.0;

    if (drawLeftEye) {
        inEye = true;
        eyeLocalPos = pos - leftCenter;
        eyeNormal = normalize(vec3(eyeLocalPos.x, -eyeLocalPos.y, leftZ));
        eyeCenterX = leftCenter.x;
    } else if (drawRightEye) {
        inEye = true;
        eyeLocalPos = pos - rightCenter;
        eyeNormal = normalize(vec3(eyeLocalPos.x, -eyeLocalPos.y, rightZ));
        eyeCenterX = rightCenter.x;
    }

    // === Build a rigid Local Eye Space basis (Independent of Camera) ===
    // We use the entity's forward direction to build a stable tangent frame.
    vec3 eyeForward = normalize(u_orbForward[orbIndex]);
    vec3 eyeUp      = (abs(eyeForward.y) > 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 eyeRight   = normalize(cross(eyeUp, eyeForward));
    vec3 eyeTrueUp  = cross(eyeForward, eyeRight);

    // Project the world gaze direction into this rigid local frame
    // (Uncomment u_gazeDir when ready. Currently using your static look direction)
    vec3 worldGaze = normalize(u_gazeDir[orbIndex]);
    worldGaze.z = -worldGaze.z;
    vec3 localGaze  = vec3(dot(worldGaze, eyeRight), dot(worldGaze, eyeTrueUp), dot(worldGaze, eyeForward));

    if (inEye) {
        float currentRadius = (drawLeftEye) ? leftEyeRadius : rightEyeRadius;
        float eyeDist = length(eyeLocalPos) / currentRadius;

        // === Eye parameters ===
        float dilation       = u_pupilDilation[orbIndex];
        float pupilRadiusMin = 0.20;
        float pupilRadiusMax = 0.60;
        float pupilRadius    = mix(pupilRadiusMin, pupilRadiusMax, dilation);

        // Sclera Base
        vec3 scleraColor = vec3(0.92, 0.90, 0.88);

        // 1. Reconstruct the true 3D View-Space surface normal of the eyeball fragment [cite: 130]
        vec3 eyeNormalView = normalize(eyeNormal.x * R - eyeNormal.y * U + eyeNormal.z * N);

        // 2. Reconstruct the world ground plane axes inside View Space using the C++ compass vector
        vec3 viewNorth = normalize(vec3(u_orbForward[orbIndex].x, 0.0, u_orbForward[orbIndex].z));
        vec3 viewSouth = -viewNorth;
        vec3 viewEast  = vec3(viewNorth.z, 0.0, -viewNorth.x); // 90-degree horizontal rotation
        vec3 viewWest  = -viewEast;

        // 3. Translate your raw World Gaze direction into View Space using this ground plane basis.
        // Combine components based on your world vector layout (X = East/West, Z = North/South)
        vec3 viewSpaceGaze = worldGaze.x * viewEast + worldGaze.z * viewSouth;
        viewSpaceGaze.y = worldGaze.y; // Keep vertical look adjustments intact
        viewSpaceGaze = normalize(viewSpaceGaze);

        // 4. Measure alignment cleanly in View Space
        float pupilDot = dot(eyeNormalView, viewSpaceGaze);

        // 5. Convert the 3D spherical alignment into your sharp circular 2D pupil mask
        float pupilDist3D = sqrt(max(0.0, 2.0 * (1.0 - pupilDot))) * 1.6;
        float pupilMask   = 1.0 - smoothstep(pupilRadius - 0.08, pupilRadius + 0.08, pupilDist3D);
        vec3  pupilColor  = vec3(0.05, 0.04, 0.03);

        // 6. Specular Highlight: Always anchored to the upper-left of the camera view
        vec3 highlightDirView = normalize(vec3(0.0, 0.0, 1.0) - R * 0.12 + U * 0.22);
        float highlightDot    = dot(eyeNormalView, highlightDirView);
        float highlightDist   = sqrt(max(0.0, 2.0 * (1.0 - highlightDot))) * 4.0;
        float highlightMask   = 1.0 - smoothstep(0.0, 0.18, highlightDist);
        vec3  highlightColor  = vec3(1.0);

        // Combine eye textures
        vec3 eyeColor = mix(scleraColor, pupilColor, pupilMask);

        // === Sun contribution — darkens with night ===
        float eyeSunDiffuse   = max(0.0, dot(eyeNormal, sunLightDir)) * sunVisibility;
        float eyeAmbientFloor = mix(0.05, 0.35, sunVisibility);
        float eyeShading      = eyeAmbientFloor + (1.0 - eyeAmbientFloor) * eyeSunDiffuse;
        float lateralShadow   = clamp(0.85 + 0.1 * sign(localSun.x) * sign(eyeCenterX) * sunVisibility, 0.0, 1.0);
        vec3  eyeSunColor     = eyeColor * mix(eyeShading, 1.0, highlightMask) * lateralShadow;
        eyeSunColor           = mix(eyeColor, highlightColor, highlightMask) * eyeShading * lateralShadow;
        eyeSunColor           *= mix(1.0, 0.15, nightFactor);

        // === Lamp contribution — independent of night darkening ===
        float eyeLampDiffuse = max(0.0, dot(eyeNormal, lampLightDir));
        float eyeShine       = pow(eyeLampDiffuse, 4.0) * lampStrength * 0.75;
        vec3  eyeLampColor   = scleraColor * eyeShine * (1.0 - pupilMask);

        // === Tapetum lucidum ===
        vec3 tapetumContrib = vec3(0.0);
        if (hasTapetum) {
            float tapetumRetro  = pow(eyeLampDiffuse, 2.0);
            float tapetumShine  = tapetumRetro * lampStrength * pupilMask * 2.5;
            tapetumContrib      = tapetumColor * clamp(tapetumShine, 0.0, 0.92);
        }

        // Combine
        eyeColor = eyeSunColor + eyeLampColor + tapetumContrib;

        // === Lid ===
        float limbBlend  = 0.0;
        
        // 2. Measure how far up/down this fragment is relative to the head's Up vector.
        // This gives us a stable, non-inverting vertical position value between -1.0 and 1.0.
        float verticalPos = dot(eyeNormalView, U);

        // 3. Map the blink parameter directly to a vertical height cutoff line.
        // When blink is 0.0, cutoff is 1.1 (completely above the eye -> fully open).
        // When blink is 1.0, cutoff is -1.1 (completely below the eye -> fully closed).
        float lidCutoff = mix(1.1, -1.1, blink);

        // 4. If the fragment's vertical position is higher than the cutoff line, 
        // the eyelid covers it. We use a smooth edge to keep it clean.
        float blinkMask = smoothstep(lidCutoff + 0.15, lidCutoff - 0.15, verticalPos);

        // Lid sun contribution 
        float lidShading  = eyeAmbientFloor + (1.0 - eyeAmbientFloor) * eyeSunDiffuse; // 
        vec3  lidSunColor = mix(scleraColor, orbColor.rgb, 0.75) * lidShading; // 
        lidSunColor       *= mix(1.0, 0.1, nightFactor); // 

        // Lid lamp contribution 
        vec3 lidLampColor = orbColor.rgb * (0.05 + 0.25 * eyeLampDiffuse) * lampStrength; // 
        vec3 lidColor = lidSunColor + lidLampColor; // 
        eyeColor = mix(eyeColor, lidColor, blinkMask); // 

        finalColor = mix(eyeColor, finalColor, limbBlend); //
    }

    fragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
}