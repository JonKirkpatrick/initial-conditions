#version 460 core

uniform float fovY;
uniform float aspectRatio;
uniform vec3  sunDir;
uniform vec4  sunColor;
uniform mat3  worldToCamMatrix;

uniform samplerCube skyCubemap;
uniform mat3        starRotationMatrix;
uniform bool        useSkyCubemap;
uniform float       skyExposure;

uniform vec3        moonDir;
uniform sampler2D   moonTexture;

uniform vec3  u_cameraPos;
uniform vec3  u_fogColorDay;
uniform vec3  u_fogColorNight;
uniform float u_fogDensity;
uniform float u_fogBaseHeight;
uniform float u_fogHeightFalloff;

in vec2 v_ndc;
out vec4 fragColor;

void main() {
    float f = tan(fovY * 0.5);
    vec3 rayDir = normalize(worldToCamMatrix * vec3(v_ndc.x * f * aspectRatio, v_ndc.y * f, -1.0));

    vec3 sunDirNorm = normalize(sunDir);
    float sunDot = max(dot(rayDir, sunDirNorm), 0.0);
    float height = rayDir.y;
    float sunElevation = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;

    // ================== ECLIPSE CALCULATION ==================
    vec3 moonDirNorm = normalize(moonDir);
    
    // Angular distance between sun and moon centers
    float sunMoonDot = dot(sunDirNorm, moonDirNorm);
    float sunMoonDist = acos(clamp(sunMoonDot, -1.0, 1.0));
    
    // Use the same angular radius defined in your moon section
    float moonRadius = 2.5 * 3.14159265 / 180.0; 
    
    // 1.0 when far apart, smoothly drops to 0.0 when perfectly aligned
    // We use a smoothstep slightly wider than the radius to dim the inner glow/disc gently
    float eclipseSunOcclusion = smoothstep(0.0, moonRadius * 1.0, sunMoonDist);

    // ================== BLEND FACTORS ==================
    float dayFactor      = smoothstep(-2.0, 12.0, sunElevation);
    dayFactor *= mix(0.02, 1.0, eclipseSunOcclusion);
    float nightFactor    = smoothstep(-8.0, -18.0, sunElevation);
    float twilightFactor = 1.0 - dayFactor - nightFactor;
    twilightFactor       = clamp(twilightFactor, 0.0, 1.0);

    // ================== SKY COLORS ==================
    vec3 dayZenith      = vec3(0.18, 0.42, 0.92);
    vec3 dayHorizon     = vec3(0.68, 0.78, 0.95);

    vec3 twilightZenith  = vec3(0.38, 0.22, 0.55);
    vec3 twilightHorizon = vec3(0.92, 0.48, 0.32);

    vec3 nightZenith    = vec3(0.008, 0.006, 0.035);
    vec3 nightHorizon   = vec3(0.025, 0.018, 0.055);

    float t = pow((height + 1.0) * 0.5, 1.35); // smoother curve

    vec3 daySky      = mix(dayHorizon, dayZenith, t);
    vec3 twilightSky = mix(twilightHorizon, twilightZenith, t);
    vec3 nightSky    = mix(nightHorizon, nightZenith, t);
    vec3 skyColor = daySky * dayFactor + twilightSky * twilightFactor + nightSky * nightFactor;

    // ================== PER-PIXEL ECLIPSE MASK ==================
    float rayMoonDot = dot(rayDir, moonDirNorm);
    float rayMoonDist = acos(clamp(rayMoonDot, -1.0, 1.0));

    // Is this specific pixel looking inside the moon's circle?
    // 0.0 inside the moon (fully blocked), 1.0 outside the moon
    float rayIsInsideMoon = smoothstep(moonRadius * 0.95, moonRadius * 1.0, rayMoonDist);

    // ================== SUN GLOW & DISC ==================
    // 1. The ultra-bright core (sunDisc) must be completely cut off by the moon's border
    float discFactor = smoothstep(-3.0, 1.0, sunElevation);
    float sunDisc = pow(sunDot, 16000.0);
    sunDisc *= rayIsInsideMoon; // Physical blocking!
    skyColor += sunDisc * vec3(1.05, 0.98, 0.85) * 3.0 * discFactor;

    // 2. The sun glow (corona). We can let a tiny bit spill around the edges,
    // but the pixel itself gets blocked if it's looking directly at the moon body.
    float sunGlow = pow(sunDot, 800.0) * 1.5;
    sunGlow *= mix(0.15, 1.0, rayIsInsideMoon); // Keeps a faint corona glow on the moon face
    skyColor += sunGlow * sunColor.rgb * sunColor.a;

    // ================== GOLDEN HOUR / TWILIGHT GLOW ==================
    float golden = pow(sunDot, 6.0) * 1.8 * twilightFactor;
    golden *= rayIsInsideMoon; 
    skyColor += golden * vec3(1.0, 0.65, 0.35);

    // ================== STARS / CUBEMAP ==================
    if (useSkyCubemap) {
        float cubemapFactor = 1.0 - smoothstep(-12.0, -3.0, sunElevation);
        vec3 starDir = starRotationMatrix * rayDir;
        // Updated legacy textureCube -> texture
        vec3 stars = texture(skyCubemap, starDir).rgb * skyExposure;
        
        // Horizon extinction
        float extinction = exp(-0.22 * (1.0 - height));
        stars *= mix(1.0, extinction, cubemapFactor);

        vec3 toneMapped = stars / (stars + vec3(1.0));
        skyColor = mix(skyColor, toneMapped, cubemapFactor * 0.95);
    }

    // ================== MOON ==================
    float moonDot = dot(rayDir, moonDirNorm);
    float moonAngDist = acos(clamp(moonDot, -1.0, 1.0));
    if (moonAngDist < moonRadius * 2.0)
    {
        vec3 safeUp = abs(moonDirNorm.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
        vec3 moonRight = normalize(cross(safeUp, moonDirNorm));
        vec3 moonUp = cross(moonDirNorm, moonRight);

        vec3 delta = rayDir - moonDirNorm * moonDot;
        float localX = dot(delta, moonRight);
        float localY = dot(delta, moonUp);
        float uvX = (localX / moonRadius) * 0.5 + 0.5;
        float uvY = (localY / moonRadius) * 0.5 + 0.5;
        vec2 uv = vec2(uvX, uvY);
        
        // Updated legacy texture2D -> texture
        vec4 moonSample = texture(moonTexture, uv);
        float boundsMask = step(0.0, uv.x) * step(0.0, uv.y) * step(uv.x, 1.0) * step(uv.y, 1.0);
        float discMask = moonSample.a * boundsMask;

        vec3 moonLightProj = sunDirNorm - moonDirNorm * dot(sunDirNorm, moonDirNorm);
        vec3 moonLight = length(moonLightProj) > 0.0001 ? normalize(moonLightProj) : moonRight;
        
        float lightX = dot(moonLight, moonRight);
        float lightY = dot(moonLight, moonUp);
        vec2 pos = vec2(uvX - 0.5, uvY - 0.5) * 2.0;
        float r2 = dot(pos, pos);
        float z = sqrt(max(1.0 - r2, 0.0));
        vec3 normal = vec3(pos.x, pos.y, z);

        float phaseAngle = acos(clamp(dot(moonDirNorm, sunDirNorm), -1.0, 1.0));
        vec3 lightDir = normalize(vec3(
            lightX * sin(phaseAngle),
            lightY * sin(phaseAngle),
            -cos(phaseAngle)
        ));
        float diffuse = max(0.0, dot(normal, lightDir));
        float earthshine = max(0.0, 1.0 - diffuse) * 0.03;
        vec3 earthColor = vec3(0.3, 0.5, 0.8);

        float limbDark = mix(0.4, 1.0, z);

        float moonElev = max(moonDirNorm.y, 0.01);
        float airmass = 1.0 / moonElev;
        vec3 rayleighBeta = vec3(0.051, 0.100, 0.275);
        vec3 moonAtmosphereColor = exp(-rayleighBeta * airmass);
        float horizonFade = smoothstep(0.0, 0.02, moonDirNorm.y);
        moonAtmosphereColor *= horizonFade;

        float ambient = mix(0.05, 0.001, dayFactor);
        vec3 bleachedTexture = mix(moonSample.rgb, vec3(0.95), dayFactor * 0.75);
        vec3 moonLitRaw = bleachedTexture * (ambient + 0.95 * diffuse) * limbDark;
        vec3 moonLit = moonLitRaw * moonAtmosphereColor;
        
        vec3 earthshineColor = earthColor * earthshine * limbDark * moonAtmosphereColor * discMask;
        vec3 moonFinal = moonLit + earthshineColor;

        float airMassRay = 1.0 / max(rayDir.y, 0.01);
        float horizonHazeFactor = 1.0 - exp(-0.08 * airMassRay);
        float dayHazeVeil = horizonHazeFactor * (dayFactor + twilightFactor * 0.5);

        float shadowMask = 1.0 - diffuse;
        float dynamicDayHaze = mix(dayHazeVeil, 1.0, dayFactor * shadowMask);
        dynamicDayHaze *= mix(0.1, 1.0, eclipseSunOcclusion);

        moonFinal = mix(moonFinal + skyColor * dayFactor * 0.4, skyColor, dynamicDayHaze);
        float moonBlend = discMask * horizonFade;
        skyColor = mix(skyColor, moonFinal, moonBlend);
    }

    // ================== HORIZON HAZE ==================
    float haze = pow(1.0 - clamp(height, -0.1, 1.0), 7.0) * 0.12;
    skyColor += haze * vec3(0.75, 0.68, 0.60) * (dayFactor * 0.6 + twilightFactor * 0.9);

    // ================== HEIGHT FOG (shared with lighting.frag) ==================
    float camHeightAboveBase = u_cameraPos.y - u_fogBaseHeight;
    float b = u_fogHeightFalloff;
    float fogAmount;
    if (rayDir.y > 0.001) {
        float verticalTerm = 1.0 / (rayDir.y * b);
        fogAmount = u_fogDensity * exp(-camHeightAboveBase * b) * verticalTerm;
    } else {
        fogAmount = 1.0;
    }
    fogAmount = clamp(fogAmount, 0.0, 1.0);

    // === ALIGNED TWILIGHT FIX HERE ===
    // Use the same elevation-based interpolation factor we used in lighting.frag
    float fogDayLightingFactor = smoothstep(-8.0, 12.0, sunElevation);
    vec3  fogColor             = mix(u_fogColorNight, u_fogColorDay, fogDayLightingFactor);

    skyColor = mix(skyColor, fogColor, fogAmount);

    fragColor = vec4(skyColor, 1.0);
}