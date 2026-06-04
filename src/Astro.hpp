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
        std::array<float, 9> starRotationMatrix; // 3x3 row-major
    };

    struct AltAz {
        float elevationRad;
        float azimuthRad;
    };

    constexpr float PI = 3.14159265f;

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
            std::cos(elevationRad) * std::sin(azimuthRad),  // X (East-West)
            std::sin(elevationRad),                         // Y (Up)
            std::cos(elevationRad) * std::cos(azimuthRad)   // Z (North-South)
        );
    }

    inline float toRad(float degrees) 
    {
        return degrees * PI / 180.0f;
    }
} // namespace Astro