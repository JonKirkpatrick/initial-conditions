#pragma once

#include <SFML/System.hpp>
#include <cmath>

// ---------------------------------------------------------------------
// WorldCoordinates
//
// Single source of truth for the invariants and conversion math shared
// by every coordinate grid built on top of raw world-space (x, z)
// meters. All grids here share one implicit origin: world (0, 0).
// No grid in this file applies an offset -- if a caller needs a
// locally-offset frame, that conversion happens at the call site, not
// in here.
// ---------------------------------------------------------------------
namespace WorldCoordinates {

    namespace Square {
        // Enforced at GIS-ingestion time by the tooling -- not a
        // per-world fact, so it does not appear in TerrainManifest.
        constexpr float kTexelSizeM      = 4.0f;   // == collision cell size
        constexpr int   kTileResolution  = 256;    // core texels/side
        constexpr int   kApronTexels     = 1;
        constexpr int   kStreamerGridDim = 11;      // CPU-resident tiles/side
        constexpr int   kVisibleGridDim  = 9;       // GPU-visible slice/side

        struct TileCoord
        {
            int row = 0;
            int col = 0;
            bool operator==(const TileCoord& other) const {
                return row == other.row && col == other.col;
            }
        };

        struct TexelCoord // also: collision grid cell
        {
            int x = 0;
            int z = 0;
        };

        TileCoord   worldPosToTileCoord(sf::Vector2f worldPos);
        int         slotIndexForTile(TileCoord coord); // toroidal %11 wrap
        TileCoord   worldPosToAbsoluteTile(sf::Vector2f worldPos, TileCoord originTile);
    }

    namespace Hex {
        constexpr float kHexSize = 1.f;

        sf::Vector2i worldToHex(float worldX, float worldZ);
        sf::Vector2f hexToWorld(int q, int r);
    }
}