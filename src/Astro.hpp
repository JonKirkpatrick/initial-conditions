#pragma once

#include <SFML/System.hpp>
#include <array>
#include <cmath>

namespace Astro {

    struct Time
    {
        double julianDate;
        double julianCenturies;
        double gmst;
        double lst;
    };

    struct State
    {
        Time astroTime;
        sf::Glsl::Vec3 sunDirection;
        sf::Glsl::Vec4 sunColor; // RGB + intensity
        sf::Glsl::Vec3 moonDirection;
        float epochOffset;
        float starRotationMatrix[9]; // 3x3 row-major
    };

    struct AltAz {
        float elevationRad;
        float azimuthRad;
    };

    struct TimeCalculationResult {
        Time astroTime;
        float epochOffset;
    };

    constexpr double PI = 3.14159265358979323846;

    inline double toRad(double degrees) 
    {
        return degrees * PI / 180.0;
    }

    inline double toDeg(double radians) 
    {
        return radians * 180.0 / PI;
    }

    inline AltAz computeAltAz(float haRad, float decRad, float latRad)
    {
        float sinElevation = std::sin(latRad) * std::sin(decRad) +
                            std::cos(latRad) * std::cos(decRad) * std::cos(haRad);
        sinElevation       = std::clamp(sinElevation, -1.0f, 1.0f);
        float elevationRad = std::asin(sinElevation);

        float azimuthRad = 0.0f;
        if (std::abs(std::cos(elevationRad)) >= 0.0001f) {
            float sinAz  = std::sin(haRad) * std::cos(decRad) / std::cos(elevationRad);
            float cosAz  = (std::sin(decRad) - std::sin(latRad) * std::sin(elevationRad)) /
                        (std::cos(latRad) * std::cos(elevationRad));
            azimuthRad   = std::atan2(sinAz, cosAz);
        }

        return { elevationRad, azimuthRad };
    }

    inline float solarDeclination(int month, int dayOfMonth)
    {
        static constexpr int daysBeforeMonth[] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int dayOfYear = daysBeforeMonth[month] + dayOfMonth;
        return 23.45f * std::sin((360.0f / 365.0f) * (dayOfYear - 81.0f) * PI / 180.0f);
    }

    inline sf::Glsl::Vec3 altAzToDirection(float elevationRad, float azimuthRad)
    {
        return sf::Glsl::Vec3(
            -std::cos(elevationRad) * std::sin(azimuthRad),  // X (East-West)
            std::sin(elevationRad),                         // Y (Up)
            -std::cos(elevationRad) * std::cos(azimuthRad)   // Z (North-South)
        );
    }

    inline TimeCalculationResult computeSiderealTime(int year, int month, int dayOfMonth, float localTime, float longitude)
    {
        double a   = std::floor((14.0 - month) / 12.0);
        double y   = year + 4800.0 - a;
        double m   = month + 12.0 * a - 3.0;
        double jdn = dayOfMonth
                   + std::floor((153.0 * m + 2.0) / 5.0)
                   + 365.0 * y
                   + std::floor(y / 4.0)
                   - std::floor(y / 100.0)
                   + std::floor(y / 400.0)
                   - 32045.0;

        // Convert local time to UTC based on pure longitude
        double utcTime      = static_cast<double>(localTime) - (static_cast<double>(longitude) / 15.0);
        double dayFraction  = (utcTime / 24.0) - 0.5;
        double daysSinceJ2000 = (jdn - 2451545.0) + dayFraction;

        Time t;
        t.julianDate      = jdn + dayFraction;
        t.julianCenturies = daysSinceJ2000 / 36525.0;

        // Calculate GMST
        t.gmst = std::fmod(280.46061837 + 360.98564736629 * daysSinceJ2000, 360.0);
        if (t.gmst < 0.0) {
            t.gmst += 360.0;
        }

        // Calculate LST
        t.lst = std::fmod(t.gmst + static_cast<double>(longitude), 360.0);
        if (t.lst < 0.0) {
            t.lst += 360.0;
        }

        // Apply texture calibration offset (-110 degrees) to the epoch offset
        constexpr double TEXTURE_CALIBRATION_DEGREES = -110.0;
        double calibratedLST = std::fmod(t.lst + TEXTURE_CALIBRATION_DEGREES, 360.0);
        if (calibratedLST < 0.0) {
            calibratedLST += 360.0;
        }

        float epochOffset = static_cast<float>(toRad(calibratedLST));

        return { t, epochOffset };
    }

    inline void computeStarRotationMatrix(float latitude, float longitude, float epochOffset, float outMatrix[9])
    {
        float lat = (90.0f - latitude) * static_cast<float>(PI) / 180.0f;
        float lon = longitude * static_cast<float>(PI) / 180.0f;
        lon -= epochOffset;

        outMatrix[0] = -std::cos(lon);
        outMatrix[1] = 0.0f;
        outMatrix[2] = -std::sin(lon);

        outMatrix[3] = std::sin(lon) * std::sin(lat);
        outMatrix[4] = std::cos(lat);
        outMatrix[5] = -std::cos(lon) * std::sin(lat);

        outMatrix[6] = std::sin(lon) * std::cos(lat);
        outMatrix[7] = -std::sin(lat);
        outMatrix[8] = -std::cos(lon) * std::cos(lat);
    }

    // Add this inside namespace Astro

    inline sf::Glsl::Vec3 computeMoonDirection(const Time& astroTime, float latitude)
    {
        double T = astroTime.julianCenturies;

        // == Fundamental arguments (Meeus chapter 47) ============================
        double Lm = std::fmod(218.3164477 + 481267.88123421 * T, 360.0); // Moon's mean longitude
        double M  = std::fmod(134.9633964 + 477198.8675055  * T, 360.0); // Moon's mean anomaly
        double Ms = std::fmod(357.5291092 + 35999.0502909   * T, 360.0); // Sun's mean anomaly
        double F  = std::fmod(93.2720950  + 483202.0175233  * T, 360.0); // Moon's argument of latitude

        // Convert to radians for trig
        double LmR = toRad(Lm);
        double MR  = toRad(M);
        double MsR = toRad(Ms);
        double FR  = toRad(F);

        // == Ecliptic longitude (degrees) — truncated Meeus series ===============
        double dL = 6288774.0 * std::sin(MR)
                  + 1274027.0 * std::sin(2.0 * LmR - MR)
                  +  658314.0 * std::sin(2.0 * LmR)
                  +  213618.0 * std::sin(2.0 * MR)
                  -  185116.0 * std::sin(MsR)
                  -  114332.0 * std::sin(2.0 * FR)
                  +   58793.0 * std::sin(2.0 * LmR - 2.0 * MR)
                  +   57066.0 * std::sin(2.0 * LmR - MsR - MR)
                  +   53322.0 * std::sin(2.0 * LmR + MR)
                  +   45758.0 * std::sin(2.0 * LmR - MsR)
                  -   40923.0 * std::sin(MsR - MR)
                  -   34720.0 * std::sin(LmR)
                  -   30383.0 * std::sin(MsR + MR);
        dL /= 1000000.0;

        // == Ecliptic latitude (degrees) ==========================================
        double dB = 5128122.0 * std::sin(FR)
                  +  280602.0 * std::sin(MR  + FR)
                  +  277693.0 * std::sin(MR  - FR)
                  +  173237.0 * std::sin(2.0 * LmR - FR)
                  +   55413.0 * std::sin(2.0 * LmR - MR + FR)
                  +   46271.0 * std::sin(2.0 * LmR - MR - FR)
                  +   32573.0 * std::sin(2.0 * LmR + FR)
                  +   17198.0 * std::sin(2.0 * MR  + FR)
                  +    9266.0 * std::sin(2.0 * LmR + MR - FR)
                  +    8822.0 * std::sin(2.0 * MR  - FR);
        dB /= 1000000.0;

        double eclLonR = toRad(std::fmod(Lm + dL, 360.0));
        double eclLatR = toRad(dB);

        // == Obliquity of ecliptic ================================================
        double eps  = 23.439291111 - 0.013004167 * T;
        double epsR = toRad(eps);

        // == Ecliptic to equatorial (RA/Dec) ======================================
        double sinDec = std::sin(eclLatR) * std::cos(epsR)
                      + std::cos(eclLatR) * std::sin(epsR) * std::sin(eclLonR);
        double decRad = std::asin(std::clamp(sinDec, -1.0, 1.0));

        double y = std::sin(eclLonR) * std::cos(epsR) - std::tan(eclLatR) * std::sin(epsR);
        double x = std::cos(eclLonR);
        double raRad = std::atan2(y, x);
        if (raRad < 0.0) {
            raRad += 2.0 * PI;
        }

        // == RA/Dec to Hour Angle =================================================
        double raDeg = toDeg(raRad);
        double haDeg = std::fmod(astroTime.lst - raDeg + 360.0, 360.0);
        float haRad  = static_cast<float>(toRad(haDeg));

        // == Hour Angle + Dec to Alt/Az ==========================================
        AltAz altaz = computeAltAz(haRad, static_cast<float>(decRad), static_cast<float>(toRad(latitude)));

        // == Convert to World Vector ==============================================
        return altAzToDirection(altaz.elevationRad, altaz.azimuthRad);
    }
} // namespace Astro