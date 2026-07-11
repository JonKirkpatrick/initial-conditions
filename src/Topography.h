#pragma once

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Topography {

    struct TerrainContext
    {
        const uint8_t* pixels;      // Direct pointer to raw RGBA pixel data
        unsigned int   width;       // Cached image width
        unsigned int   height;      // Cached image height
        sf::Vector2f   worldMin;
        sf::Vector2f   worldSize;
        float          maxHeight;
    };

    constexpr int GRID_RESOLUTION = 1280;
    constexpr float BASE_SIZE = 1024.0f * 5.0f;

    float heightAt(const TerrainContext& ctx, float worldX, float worldZ);
    sf::Vector3f normalAt(const TerrainContext& ctx, float worldX, float worldZ);

} // namespace Topography