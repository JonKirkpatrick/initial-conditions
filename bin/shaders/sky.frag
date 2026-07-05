uniform vec2 viewportSize;
uniform float fovY;
uniform float aspectRatio;
uniform vec3 sunDir;
uniform vec4 sunColor;
uniform mat3 worldToCamMatrix;

uniform samplerCube skyCubemap;
uniform mat3 starRotationMatrix;
uniform bool useSkyCubemap;
uniform float skyExposure;

uniform vec3 moonDir;
uniform sampler2D moonTexture;

void main() {
    vec2 screen = gl_FragCoord.xy;
    screen.y = viewportSize.y - screen.y;
    float x_ndc = (screen.x / viewportSize.x) * 2.0 - 1.0;
    float y_ndc = 1.0 - (screen.y / viewportSize.y) * 2.0;
    float f = tan(fovY * 0.5);
    vec3 rayDir = normalize(worldToCamMatrix * vec3(x_ndc * f * aspectRatio, y_ndc * f, -1.0));

    vec3 sunDirNorm = normalize(sunDir);
    float sunDot = max(dot(rayDir, sunDirNorm), 0.0);
    float height = rayDir.y;
    float sunElevation = asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / 3.14159265;

    // ================== BLEND FACTORS ==================
    float dayFactor      = smoothstep(-2.0, 12.0, sunElevation);
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

    float t = pow((height + 1.0) * 0.5, 1.35);   // smoother curve

    vec3 daySky      = mix(dayHorizon, dayZenith, t);
    vec3 twilightSky = mix(twilightHorizon, twilightZenith, t);
    vec3 nightSky    = mix(nightHorizon, nightZenith, t);

    vec3 skyColor = daySky * dayFactor + twilightSky * twilightFactor + nightSky * nightFactor;

    // ================== SUN GLOW & DISC ==================
    float sunGlow = pow(sunDot, 18.0) * 4.5;
    skyColor += sunGlow * sunColor.rgb * sunColor.a;

    float discFactor = smoothstep(-3.0, 4.0, sunElevation);
    float sunDisc = pow(sunDot, 160.0);
    skyColor += sunDisc * vec3(1.05, 0.98, 0.85) * 3.0 * discFactor;

    // ================== GOLDEN HOUR / TWILIGHT GLOW ==================
    float golden = pow(sunDot, 6.0) * 1.8 * twilightFactor;
    skyColor += golden * vec3(1.0, 0.65, 0.35);

    // ================== STARS / CUBEMAP ==================
    if (useSkyCubemap) {
        float cubemapFactor = 1.0 - smoothstep(-12.0, -3.0, sunElevation);
        
        vec3 starDir = starRotationMatrix * rayDir;
        vec3 stars = textureCube(skyCubemap, starDir).rgb * skyExposure;

        // Horizon extinction
        float extinction = exp(-0.22 * (1.0 - height));
        stars *= mix(1.0, extinction, cubemapFactor);

        vec3 toneMapped = stars / (stars + vec3(1.0));
        skyColor = mix(skyColor, toneMapped, cubemapFactor * 0.95);
    }

    // ================== MOON ==================
    vec3 moonDirNorm = normalize(moonDir);
    float moonDot = dot(rayDir, moonDirNorm);
    float moonAngDist = acos(clamp(moonDot, -1.0, 1.0));
    float moonAngRadius = 2.5 * 3.14159265 / 180.0;

    if (moonAngDist < moonAngRadius * 2.0)
    {
        // == Build a local tangent frame around the moon center ==================
        // == Stable branchless billboard frame ===================================
        // Use a vector that changes based on orientation to prevent cross-product collapse
        vec3 safeUp = abs(moonDirNorm.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
        vec3 moonRight = normalize(cross(safeUp, moonDirNorm));
        vec3 moonUp = cross(moonDirNorm, moonRight); // Already normalized if right and dir are normalized

        // == Project rayDir onto moon's local frame ==============================
        vec3 delta = rayDir - moonDirNorm * moonDot;
        float localX = dot(delta, moonRight);
        float localY = dot(delta, moonUp);

        // == Convert to UV (0..1) ================================================
        float uvX = (localX / moonAngRadius) * 0.5 + 0.5;
        float uvY = (localY / moonAngRadius) * 0.5 + 0.5;
        vec2 uv = vec2(uvX, uvY);

        // == Sample the texture ==================================================
        vec4 moonSample = texture2D(moonTexture, uv);
        
        // CRITICAL FIX: Zero out alpha if UV coordinates step outside the texture bounds
        float boundsMask = step(0.0, uv.x) * step(0.0, uv.y) * step(uv.x, 1.0) * step(uv.y, 1.0);
        float discMask = moonSample.a * boundsMask;

        // == Create Moonlight (3D Diffuse Lighting) ==============================
        vec3 moonLightProj = sunDirNorm - moonDirNorm * dot(sunDirNorm, moonDirNorm);
        
        // Prevent normalization crash if sun and moon are perfectly aligned
        vec3 moonLight = length(moonLightProj) > 0.0001 ? normalize(moonLightProj) : moonRight;
        
        float lightX = dot(moonLight, moonRight);
        float lightY = dot(moonLight, moonUp);

        vec2 pos = vec2(uvX - 0.5, uvY - 0.5) * 2.0;
        float r2 = dot(pos, pos);
        
        // Ensure we stay inside the sphere's bounds before square rooting
        float z = sqrt(max(1.0 - r2, 0.0)); 
        vec3 normal = vec3(pos.x, pos.y, z);

        float phaseAngle = acos(clamp(dot(moonDirNorm, sunDirNorm), -1.0, 1.0));
        vec3 lightDir = normalize(vec3(
            lightX * sin(phaseAngle),
            lightY * sin(phaseAngle),
            -cos(phaseAngle)
        ));

        // This diffuse value replaces the old 'litFactor'
        float diffuse = max(0.0, dot(normal, lightDir));

        // == Earthshine — faint blue glow on dark (unlit) limb ===================
        // We use (1.0 - diffuse) instead of the old 2D (1.0 - litFactor)
        float earthshine = max(0.0, 1.0 - diffuse) * 0.03;
        vec3 earthColor = vec3(0.3, 0.5, 0.8);

        // == Limb darkening ======================================================
        float limbDark = mix(0.4, 1.0, z); // z is equivalent to sqrt(1.0 - r^2)

        // == Atmospheric extinction & Rayleigh Scattering near horizon ===========
        float moonElev = max(moonDirNorm.y, 0.01);
        float airmass = 1.0 / moonElev;
        vec3 rayleighBeta = vec3(0.051, 0.100, 0.275);
        vec3 moonAtmosphereColor = exp(-rayleighBeta * airmass);
        float horizonFade = smoothstep(0.0, 0.02, moonDirNorm.y);
        moonAtmosphereColor *= horizonFade;

        // == Assemble final moon color ============================================
        // Drop ambient to near-zero during the day so the texture shadows don't stay dark grey
        float ambient = mix(0.05, 0.001, dayFactor);
        
        // 1. DAYTIME BLEACH: Bleach and desaturate the raw moon texture details during the day
        // This mixes the texture with a flat white base, fading out deep craters and maria
        vec3 bleachedTexture = mix(moonSample.rgb, vec3(0.95), dayFactor * 0.75);
        
        // 2. Apply 3D diffuse lighting and limb darkening to the bleached base
        vec3 moonLitRaw = bleachedTexture * (ambient + 0.95 * diffuse) * limbDark;
        vec3 moonLit = moonLitRaw * moonAtmosphereColor;
        
        // Earthshine (only visible at night)
        vec3 earthshineColor = earthColor * earthshine * limbDark * moonAtmosphereColor * discMask;
        vec3 moonFinal = moonLit + earthshineColor;

        // == Dynamic Daytime Distance Haze (The Secret Sauce) ====================
        float airMassRay = 1.0 / max(rayDir.y, 0.01);
        float horizonHazeFactor = 1.0 - exp(-0.08 * airMassRay);
        float dayHazeVeil = horizonHazeFactor * (dayFactor + twilightFactor * 0.5);

        // For the unlit portions, force it completely to 1.0 to remain invisible
        float shadowMask = 1.0 - diffuse;
        float dynamicDayHaze = mix(dayHazeVeil, 1.0, dayFactor * shadowMask);

        // 3. ADDITIVE SCATTERING VEIL: Instead of just interpolating, 
        // physically inject sky brightness over the lit moon crescent.
        moonFinal = mix(moonFinal + skyColor * dayFactor * 0.4, skyColor, dynamicDayHaze);

        // == Blend moon over sky =================================================
        float moonBlend = discMask * horizonFade;
        skyColor = mix(skyColor, moonFinal, moonBlend);
    }

    // ================== HORIZON HAZE ==================
    float haze = pow(1.0 - clamp(height, -0.1, 1.0), 7.0) * 0.12;
    skyColor += haze * vec3(0.75, 0.68, 0.60) * (dayFactor * 0.6 + twilightFactor * 0.9);

    gl_FragColor = vec4(skyColor, 1.0);
}
