#pragma once

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace Topography {

    // ============================================================================
    // Terrain Data Structures
    // ============================================================================

    struct TerrainContext
    {
        const sf::Image& heightmap;
        sf::Vector2f worldMin;
        sf::Vector2f worldSize;
        float maxHeight;
    };

    constexpr int GRID_RESOLUTION = 300; // Matches your Python PIXELS = 300
    constexpr float BASE_SIZE = 93800.0f; // 938 meters in centimeters

    // ============================================================================
    // Terrain Query Functions
    // ============================================================================

    float heightAt(const TerrainContext& ctx, float worldX, float worldZ);
    sf::Vector3f normalAt(const TerrainContext& ctx, float worldX, float worldZ);

} // namespace Topography
