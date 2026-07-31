#pragma once

#include <SFML/System.hpp>
#include <cmath>

/**
 * @brief Single source of truth for spatial invariants and coordinate transformations.
 * 
 * Provides static constants and unit conversion functions for square tile grids, 
 * collision cells, and hexagonal coordinate systems built on top of continuous 
 * world-space coordinates in meters \f$(x, z)\f$.
 * 
 * @note All grids share an implicit un-offset origin at world point \f$(0, 0)\f$. 
 * If a caller requires a locally-offset frame, that conversion must take place at the call site.
 */
namespace WorldCoordinates {

    /**
     * @brief Square tile, collision cell, and streaming terrain grid coordinate conversions.
     */
    namespace Square {
        /** @brief Length of a single texel / collision cell side in world meters (4.0m). */
        constexpr float kTexelSizeM      = 4.0f;

        /** @brief Number of core texels along one edge of a terrain tile (256). */
        constexpr int   kTileResolution  = 256;

        /** @brief Number of extra border texels wrapping tile edges for seamless sampling. */
        constexpr int   kApronTexels     = 1;

        /** @brief Dimension (in tiles) of the CPU streaming grid cache (\f$11 \times 11\f$ tiles). */
        constexpr int   kStreamerGridDim = 11;

        /** @brief Dimension (in tiles) of the GPU-visible rendering slice (\f$9 \times 9\f$ tiles). */
        constexpr int   kVisibleGridDim  = 9;

        /**
         * @brief Integer grid coordinates identifying a specific terrain tile.
         */
        struct TileCoord
        {
            int row = 0; ///< Tile row index along the Z-axis.
            int col = 0; ///< Tile column index along the X-axis.

            /**
             * @brief Equality operator for tile coordinates.
             * @param other The coordinate to compare against.
             * @return `true` if row and col match, `false` otherwise.
             */
            bool operator==(const TileCoord& other) const {
                return row == other.row && col == other.col;
            }
        };

        /**
         * @brief Discrete texel coordinate within the terrain or collision grid.
         */
        struct TexelCoord
        {
            int x = 0; ///< Texel index along the X-axis.
            int z = 0; ///< Texel index along the Z-axis.
        };

        /**
         * @brief Converts continuous 2D world coordinates to a tile grid coordinate.
         * @param worldPos 2D continuous world position vector in meters.
         * @return `TileCoord` containing the integer tile indices containing the point.
         */
        TileCoord   worldPosToTileCoord(sf::Vector2f worldPos);

        /**
         * @brief Computes the toroidal cache array slot index for a given tile.
         * 
         * Maps tile coordinates into the \f$11 \times 11\f$ streamer grid using a modulo wrap operation.
         * @param coord The absolute tile coordinate.
         * @return Linearized array slot index.
         */
        int         slotIndexForTile(TileCoord coord);

        /**
         * @brief Computes absolute tile coordinates relative to a streamer origin tile.
         * @param worldPos 2D continuous world position vector in meters.
         * @param originTile The reference anchor tile coordinate.
         * @return Absolute `TileCoord` offset from origin.
         */
        TileCoord   worldPosToAbsoluteTile(sf::Vector2f worldPos, TileCoord originTile);
    }

    /**
     * @brief Hexagonal grid spatial calculations and coordinate conversions.
     */
    namespace Hex {
        /** @brief Outer radius/side length of a single hexagon unit in meters. */
        constexpr float kHexSize = 1.f;

        /**
         * @brief Converts continuous world coordinates to discrete axial hex coordinates \f$(q, r)\f$.
         * @param worldX Continuous world X position in meters.
         * @param worldZ Continuous world Z position in meters.
         * @return 2D integer vector containing axial hex coordinates \f$(q, r)\f$.
         */
        sf::Vector2i worldToHex(float worldX, float worldZ);

        /**
         * @brief Converts axial hex coordinates \f$(q, r)\f$ to continuous world coordinates.
         * @param q Axial column index.
         * @param r Axial row index.
         * @return 2D world position vector corresponding to the center of the hexagon.
         */
        sf::Vector2f hexToWorld(int q, int r);
    }
}