uniform vec3 sunDir;
uniform vec3 sunDirWorld;
uniform vec4 sunColor;
uniform sampler2D u_bakeTex;
uniform vec2 u_viewportSize;
uniform float headlampIntensity;
uniform float headlampRange;
uniform float headlampConeCos; // cos(angle) of headlamp cone (for cutoff)
uniform float headlampEnabled; // 1.0 when headlights are on, 0.0 when off

// === Batching support ===
uniform int u_batchSize;
uniform vec3  u_orbCenterView[64];
uniform vec4  u_orbColor[64];
uniform float u_orbDepthNorm[64];   // the normalized depth into the scene
uniform vec2  u_quadOrigin[64];     // top left of orb in screen space
uniform vec2  u_texSize[64];        // diameter in pixels per orb

void main()
{
    // Find which orb this fragment belongs to
    int orbIndex = int(gl_Color.r * 255.0 + 0.5);

    if (orbIndex >= u_batchSize) {
        discard;
    }
    
    vec2 quadOrigin = u_quadOrigin[orbIndex];
    vec2 texSize    = u_texSize[orbIndex];
    vec3 orbCenter  = u_orbCenterView[orbIndex];
    float depthNorm = u_orbDepthNorm[orbIndex];
    vec4 orbColor   = u_orbColor[orbIndex];

    // Convert gl_FragCoord (OpenGL bottom-left Y-up) to SFML window coords (top-left Y-down)
    vec2 fragScreenSFML = vec2(gl_FragCoord.x, u_viewportSize.y - gl_FragCoord.y);

    // Local position inside the billboard quad (SFML coords)
    vec2 localSFML = fragScreenSFML - quadOrigin;
    vec2 uv = localSFML / texSize;

    // Per-pixel occlusion using screen position
    vec2 fragScreenUv = fragScreenSFML / u_viewportSize;
    fragScreenUv.y = 1.0 - fragScreenUv.y;

    // Here "norm" is refering to normalized.  It's how deep in the scene the pixel is.
    vec4 topo = texture2D(u_bakeTex, fragScreenUv);
    if (topo.a >= 0.5) {
        float terrainNorm = topo.r + topo.g / 255.0 + topo.b / (255.0 * 255.0);
        if (terrainNorm + 0.0008 < depthNorm) {
            discard;
        }
    }

    vec2 pos = (uv - vec2(0.5)) * 2.0;
    float dist = length(pos);
    
    if (dist > 1.0) {
        discard;
    }
    
    float z = sqrt(1.0 - dist * dist);
    vec3 normal = normalize(vec3(pos.x, -pos.y, z));
    
    // Two-source sphere shading: sun + camera headlamp.
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 sunLightDir = normalize(sunDir);
    float sunDiffuse = max(0.0, dot(normal, sunLightDir));
    vec3 sunHalfDir = normalize(sunLightDir + viewDir);

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
    vec3 lampHalfDir = normalize(lampLightDir + viewDir);
    vec3 flashlightShaded = orbColor.rgb * (0.15 + 0.85 * lampDiffuse);
    flashlightShaded *= lampStrength;

    // Atmospheric perspective: mirror topo_final's smooth dusk/night rolloff.
    float atmosphereMaxDist = 0.5;
    float distNormalized = clamp(depthNorm / atmosphereMaxDist, 0.0, 1.0);
    float atmosphereStrength = pow(distNormalized, 1.7);

    vec3 sunDirNorm = sunWorldDir;

    float dayMix = smoothstep(-10.0, 6.0, sunElevationDeg);
    vec3 shaded = ambient;
    shaded += (sunShaded - ambient) * dayMix;
    shaded += flashlightShaded;

    vec3 nightAtm = vec3(0.008, 0.006, 0.025);
    vec3 atmTint = mix(sunColor.rgb * vec3(0.7, 0.65, 0.8),
                       vec3(0.50, 0.68, 0.95), 0.6);
    float daytimeFactor = clamp(sunDirNorm.y * 1.8, 0.0, 1.0);
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

    gl_FragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
}
