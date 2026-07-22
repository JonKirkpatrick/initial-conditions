const float PI = 3.14159265;
const float MOON_ANGULAR_RADIUS = 2.5 * PI / 180.0;

struct DayNightFactors { float day, twilight, night; };

float sunElevationDegrees(vec3 sunDirNorm) {
    return asin(clamp(sunDirNorm.y, -1.0, 1.0)) * 180.0 / PI;
}

float dayLightingFactor(float elevationDeg)
{
    return smoothstep(-8.0, 12.0, elevationDeg);
}

DayNightFactors computeDayNightFactors(float elevationDeg)
{
    DayNightFactors factors;
    factors.day       = smoothstep(-2.0, 12.0, elevationDeg);
    factors.night     = 1.0 - smoothstep(-18.0, -8.0, elevationDeg);
    factors.twilight  = clamp(1.0 - factors.day - factors.night, 0.0, 1.0);

    return factors;
}