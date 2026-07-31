#pragma once

/**
 * @file Topography.h
 * @brief Topographical evaluation routines and terrain sampling contexts.
 * 
 * Provides utility functions for sampling terrain heightfields and evaluating 
 * analytical surface normals in world space.
 */

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include "environment/TerrainStreamer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Topography {

    /**
     * @brief Context container binding terrain streamer instance with world-space bounds.
     */
    struct TerrainContext
    {
        const TerrainStreamer* streamer;  ///< Pointer to active terrain streaming manager.
        sf::Vector2f           worldMin;  ///< Minimum (top-left) world-space coordinate bounds.
        sf::Vector2f           worldSize; ///< Total extents (width, height) of active terrain area in world units.
    };

    /** @brief Grid resolution dimension for terrain sampling mesh grid. */
    constexpr int GRID_RESOLUTION = 2304;

    /** @brief Total side length extent in world units for the $9 \times 9$ subgrid region. */
    constexpr float BASE_SIZE = 1024.0f * 9.0f;

    /**
     * @brief Samples terrain height at a specific 2D world-space position.
     * 
     * @param ctx Active terrain context containing streamer reference and world bounds.
     * @param worldPos 2D position in world space.
     * @return Interpolated terrain height in meters (or $0.0\text{f}$ if unmapped/invalid).
     */
    float heightAt(const TerrainContext& ctx, sf::Vector2f worldPos);

    /**
     * @brief Calculates surface normal vector at a specific 2D world-space position.
     * 
     * Evaluates neighborhood height gradients to compute a normalized 3D surface normal.
     * 
     * @param ctx Active terrain context containing streamer reference and world bounds.
     * @param worldPos 2D position in world space.
     * @return Normalized 3D surface normal vector $(N_x, N_y, N_z)$.
     */
    sf::Vector3f normalAt(const TerrainContext& ctx, sf::Vector2f worldPos);

} // namespace Topography