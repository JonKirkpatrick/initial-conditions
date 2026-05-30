uniform vec2 viewportSize;
uniform float fovY;
uniform float aspectRatio;
uniform vec3 sunDir;
uniform vec4 sunColor;
uniform mat3 invRotationMatrix;

uniform samplerCube skyCubemap;
uniform bool useSkyCubemap;
uniform float skyExposure;

void main() {
    vec2 screen = gl_FragCoord.xy;
    screen.y = viewportSize.y - screen.y;
    float x_ndc = (screen.x / viewportSize.x) * 2.0 - 1.0;
    float y_ndc = 1.0 - (screen.y / viewportSize.y) * 2.0;
    float f = tan(fovY * 0.5);
    vec3 rayDir = normalize(invRotationMatrix * vec3(x_ndc * f * aspectRatio, y_ndc * f, -1.0));

    vec3 sunDirNorm = normalize(sunDir);
    float sunDot = max(dot(rayDir, sunDirNorm), 0.0);
    float height = rayDir.y;
    float sunElevation = asin(sunDirNorm.y) * 180.0 / 3.14159265;

    // ================== BLEND FACTORS ==================
    // dayFactor:      0=not day,      1=full day      (transitions over 5-10 degrees)
    // twilightFactor: 0=not twilight, 1=full twilight (peaks around 0 degrees)
    // nightFactor:    0=not night,    1=full night    (transitions over -5 to -10 degrees)
    float dayFactor      = smoothstep(0.0, 10.0, sunElevation);
    float nightFactor    = smoothstep(-5.0, -12.0, sunElevation);  // note: inverted range
    float twilightFactor = 1.0 - dayFactor - nightFactor;          // peaks in the middle
    twilightFactor       = clamp(twilightFactor, 0.0, 1.0);

    // ================== SKY STATES ==================
    vec3 dayZenith      = vec3(0.25, 0.45, 0.95);
    vec3 dayHorizon     = vec3(0.70, 0.80, 0.95);
    vec3 twilightZenith  = vec3(0.45, 0.25, 0.65);
    vec3 twilightHorizon = vec3(0.95, 0.55, 0.35);
    vec3 nightColor     = vec3(0.02, 0.01, 0.08);

    // Compute each state independently
    float tDay      = pow((height + 1.0) * 0.5, 1.4);
    float tTwilight = pow((height + 1.0) * 0.5, 1.6);

    vec3 daySky      = mix(dayHorizon, dayZenith, tDay);
    vec3 twilightSky = mix(twilightHorizon, twilightZenith, tTwilight);
    vec3 nightSky    = nightColor;

    // Blend all three together
    vec3 skyColor = daySky      * dayFactor
                  + twilightSky * twilightFactor
                  + nightSky    * nightFactor;

    // ================== GOLDEN HOUR GLOW ==================
    // Fades in with twilight, fades out with day/night
    float golden = pow(sunDot, 8.0) * 0.8 * twilightFactor;
    skyColor += golden * vec3(1.0, 0.7, 0.4);

    // ================== SUN / ATMOSPHERE ==================
    float sunGlow = pow(sunDot, 24.0) * 3.5;
    vec3 sunLight = sunColor.rgb * sunColor.a;
    skyColor += sunGlow * sunLight * 0.9;

    // Sun disc fades in smoothly as sun clears horizon
    float discFactor = smoothstep(-2.0, 2.0, sunElevation);
    float sunDisc = pow(sunDot, 180.0);
    skyColor += sunDisc * vec3(1.0, 0.95, 0.82) * 2.0 * discFactor;

    // ================== CUBEMAP STARS / DEEP SKY ==================
    if (useSkyCubemap) {
        float cubemapFactor = 1.0 - smoothstep(-10.0, -2.0, sunElevation);
        
        // 1. Grab raw data and apply exposure
        vec3 exposedColor = textureCube(skyCubemap, rayDir).rgb * skyExposure;

        // 2. Atmospheric Extinction (Dim stars near the horizon)
        float airMass = 1.0 / max(rayDir.y, 0.01); 
        float extinction = exp(-0.15 * airMass); 
        exposedColor *= mix(1.0, extinction, cubemapFactor);

        // 3. Tone Mapping (Keep your 32-bit star profiles crisp and sharp)
        vec3 toneMappedColor = exposedColor / (exposedColor + vec3(1.0));

        // 4. Blend natively
        skyColor = mix(skyColor, toneMappedColor, cubemapFactor);
    }

    // ================== HORIZON HAZE ==================
    float haze = pow(1.0 - clamp(height, 0.0, 1.0), 8.0) * 0.08;
    skyColor += haze * vec3(0.72, 0.66, 0.58) * 0.25;

    gl_FragColor = vec4(skyColor, 1.0);
}
