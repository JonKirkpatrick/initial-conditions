#pragma once

/**
 * @file Astro.hpp
 * @brief Astronomical mechanics, celestial positioning, and temporal tracking system.
 * 
 * Provides analytical routines for calculating solar position, lunar positions using 
 * truncated Meeus series, sidereal time (GMST/LST), starfield rotation matrices, 
 * and day/night light coloration curves.
 */

#include <SFML/System.hpp>
#include <array>
#include <cmath>

namespace Astro {

    /**
     * @brief Astronomical time metrics.
     */
    struct Time
    {
        double julianDate;      ///< Full Julian Date (JDN + time fraction).
        double julianCenturies; ///< Julian centuries elapsed since epoch J2000.0.
        double gmst;            ///< Greenwich Mean Sidereal Time in degrees [0, 360).
        double lst;             ///< Local Sidereal Time in degrees [0, 360).
    };

    /**
     * @brief Complete snapshot of celestial environment state for rendering.
     */
    struct State
    {
        Time astroTime;                  ///< Astronomical time parameters.
        sf::Glsl::Vec3 sunDirection;     ///< Normalized unit vector pointing to the sun in world coordinates.
        sf::Glsl::Vec4 sunColor;         ///< Direct sunlight RGB color + intensity factor (W).
        sf::Glsl::Vec3 moonDirection;    ///< Normalized unit vector pointing to the moon in world coordinates.
        float epochOffset;               ///< Calibrated epoch angle in radians for starfield alignment.
        float starRotationMatrix[9];     ///< 3x3 row-major transformation matrix for orientation of background star sphere.
    };

    /**
     * @brief Horizontal coordinate system values (Altitude/Azimuth).
     */
    struct AltAz {
        float elevationRad; ///< Elevation / Altitude angle in radians above the horizon.
        float azimuthRad;   ///< Azimuth angle in radians measured eastwards from North.
    };

    /**
     * @brief Intermediate container returning calculated sidereal time and epoch calibration.
     */
    struct TimeCalculationResult {
        Time astroTime;     ///< Populated astronomical time structure.
        float epochOffset;  ///< Calibrated starfield alignment offset in radians.
    };

    /** @brief Mathematical constant $\pi$. */
    constexpr double PI = 3.14159265358979323846;

    /**
     * @brief Converts angular degrees to radians.
     * @param degrees Angle in degrees.
     * @return Angle in radians.
     */
    inline double toRad(double degrees) 
    {
        return degrees * PI / 180.0;
    }

    /**
     * @brief Converts angular radians to degrees.
     * @param radians Angle in radians.
     * @return Angle in degrees.
     */
    inline double toDeg(double radians) 
    {
        return radians * 180.0 / PI;
    }

    /**
     * @brief Computes Horizontal coordinates (Altitude/Azimuth) from Equatorial parameters.
     * 
     * @param haRad Hour angle in radians.
     * @param decRad Declination in radians.
     * @param latRad Geographic observer latitude in radians.
     * @return Calculated `AltAz` structure.
     */
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

    /**
     * @brief Approximates solar declination angle for a given calendar day.
     * 
     * @param month Current calendar month (1–12).
     * @param dayOfMonth Current day of month (1–31).
     * @return Solar declination angle in degrees.
     */
    inline float solarDeclination(int month, int dayOfMonth)
    {
        static constexpr int daysBeforeMonth[] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int dayOfYear = daysBeforeMonth[month] + dayOfMonth;
        return 23.45f * std::sin((360.0f / 365.0f) * (dayOfYear - 81.0f) * PI / 180.0f);
    }

    /**
     * @brief Converts horizontal spherical coordinates (Altitude/Azimuth) to a normalized 3D direction vector.
     * 
     * Mapping convention:
     * - X: East-West axis
     * - Y: Vertical / Up axis
     * - Z: North-South axis
     * 
     * @param elevationRad Elevation angle in radians.
     * @param azimuthRad Azimuth angle in radians.
     * @return 3D normalized unit vector in world space.
     */
    inline sf::Glsl::Vec3 altAzToDirection(float elevationRad, float azimuthRad)
    {
        return sf::Glsl::Vec3(
            -std::cos(elevationRad) * std::sin(azimuthRad),  // X (East-West)
            std::sin(elevationRad),                         // Y (Up)
            -std::cos(elevationRad) * std::cos(azimuthRad)   // Z (North-South)
        );
    }

    /**
     * @brief Calculates astronomical time variables (Julian Date, GMST, LST) based on observer location and time.
     * 
     * @param year Full calendar year (e.g. 2026).
     * @param month Month (1–12).
     * @param dayOfMonth Day of month (1–31).
     * @param localTime Fractional solar hour of day [0.0, 24.0).
     * @param longitude Observer geographic longitude in degrees (East positive, West negative).
     * @return Calculated `TimeCalculationResult` struct containing `Time` metrics and `epochOffset`.
     */
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

    /**
     * @brief Computes a 3x3 rotation matrix for transforming background skybox star coordinates.
     * 
     * @param latitude Observer latitude in degrees.
     * @param longitude Observer longitude in degrees.
     * @param epochOffset Starfield texture alignment angle in radians.
     * @param[out] outMatrix Array of 9 floats receiving the 3x3 row-major transformation matrix.
     */
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

    /**
     * @brief Computes the 3D world direction vector pointing toward the Moon.
     * 
     * Uses a truncated Jean Meeus analytical series (*Astronomical Algorithms*, Ch. 47) 
     * to evaluate lunar ecliptic coordinates, converts to equatorial RA/Dec, and maps 
     * to local Alt/Az direction vectors.
     * 
     * @param astroTime Current astronomical time parameter context.
     * @param latitude Observer geographic latitude in degrees.
     * @return 3D normalized direction vector toward the moon.
     */
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

    /**
     * @brief Computes complete astronomical system environment state for given date/time and location.
     * 
     * Evaluates sun position/color temperature, sidereal time parameters, celestial star matrix, 
     * and analytical lunar direction vector.
     * 
     * @param year Calendar year.
     * @param month Month (1–12).
     * @param dayOfMonth Day of month (1–31).
     * @param timeOfDay Time of day in decimal hours [0.0, 24.0).
     * @param latitude Observer latitude in degrees.
     * @param longitude Observer longitude in degrees.
     * @return Fully populated `State` structure for sky rendering and directional lighting.
     */
    inline State calculateState(int year, int month, int dayOfMonth, double timeOfDay, float latitude, float longitude)
    {
        State state;

        // 1. Sidereal Time calculations
        auto timeResult = computeSiderealTime(year, month, dayOfMonth, static_cast<float>(timeOfDay), longitude);
        state.astroTime   = timeResult.astroTime;
        state.epochOffset = timeResult.epochOffset;

        // 2. Sun Direction & Intensity calculations
        float declination = solarDeclination(month, dayOfMonth);
        float haDeg       = (static_cast<float>(timeOfDay) / 24.0f - 0.5f) * 360.0f;

        AltAz sunAltAz = computeAltAz(
            static_cast<float>(toRad(haDeg)),
            static_cast<float>(toRad(declination)),
            static_cast<float>(toRad(latitude))
        );

        state.sunDirection = altAzToDirection(sunAltAz.elevationRad, sunAltAz.azimuthRad);

        // Sun Color & Warming curve
        float elevationDeg    = static_cast<float>(toDeg(sunAltAz.elevationRad));
        float sunHeightFactor = std::clamp((elevationDeg + 12.0f) / 90.0f, 0.0f, 1.0f);
        float warmth          = 1.0f - sunHeightFactor * 0.75f;

        state.sunColor = sf::Glsl::Vec4(
            1.00f,
            0.90f + warmth * 0.10f,
            0.65f + warmth * 0.30f,
            sunHeightFactor * 1.25f + 0.25f
        );

        // 3. Starfield Background Orientation matrix
        computeStarRotationMatrix(latitude, longitude, state.epochOffset, state.starRotationMatrix);

        // 4. Analytical Lunar Positioning
        state.moonDirection = computeMoonDirection(state.astroTime, latitude);

        return state;
    }

    /**
     * @brief Advances in-game calendar and clock state based on delta time.
     * 
     * Handles progression of fractional hours, month length variations, leap years, 
     * and year rollovers.
     * 
     * @param dt Frame delta time in seconds.
     * @param realSecondsPerHour Time scale ratio (real seconds representing one in-game hour).
     * @param[in,out] gameTime In-game clock in hours [0.0, 24.0).
     * @param[in,out] day Day of month (1–31).
     * @param[in,out] month Month (1–12).
     * @param[in,out] year Full calendar year.
     */
    inline void advanceCalendar(double dt, double realSecondsPerHour, 
                                double& gameTime, int& day, int& month, int& year) 
    {
        gameTime += dt * (1.0 / realSecondsPerHour);
        if (gameTime >= 24.0) {
            gameTime -= 24.0;
            day++;
            
            static const int daysInMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
            bool leapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            int daysThisMonth = daysInMonth[month] + ((leapYear && month == 2) ? 1 : 0);

            if (day > daysThisMonth) {
                day = 1;
                month++;
                if (month > 12) {
                    month = 1;
                    year++;
                }
            }
        }
    }
} // namespace Astro