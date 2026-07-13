#pragma once

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>

struct HeightArray; // defined in Assets.h; TerrainContext only needs a pointer here

namespace Topography {

    struct TerrainContext
    {
        const HeightArray* heightArray;  // owns width, height, and the flat float buffer
        sf::Vector2f        worldMin;
        sf::Vector2f        worldSize;
    };

    constexpr int GRID_RESOLUTION = 1280;
    constexpr float BASE_SIZE = 1024.0f * 5.0f;

    float heightAt(const TerrainContext& ctx, float worldX, float worldZ);
    sf::Vector3f normalAt(const TerrainContext& ctx, float worldX, float worldZ);

} // namespace Topography