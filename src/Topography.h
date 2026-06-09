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

    // ============================================================================
    // Terrain Query Functions
    // ============================================================================

    float heightAt(const TerrainContext& ctx, float worldX, float worldZ);
    sf::Vector3f normalAt(const TerrainContext& ctx, float worldX, float worldZ);

} // namespace Topography
