#pragma once

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include "TerrainStreamer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Topography {

    struct TerrainContext
    {
        const TerrainStreamer* streamer;
        sf::Vector2f           worldMin;
        sf::Vector2f           worldSize;
    };

    constexpr int GRID_RESOLUTION = 1280;
    constexpr float BASE_SIZE = 1024.0f * 5.0f;

    float heightAt(const TerrainContext& ctx, sf::Vector2f worldPos);
    sf::Vector3f normalAt(const TerrainContext& ctx, sf::Vector2f worldPos);

} // namespace Topography