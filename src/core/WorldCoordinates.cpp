#include "core/WorldCoordinates.hpp"

namespace WorldCoordinates::Square {

    TileCoord worldPosToTileCoord(sf::Vector2f worldPos)
    {
        constexpr float kTileSizeM = kTexelSizeM * kTileResolution;
        int row = static_cast<int>(std::floor(worldPos.y / kTileSizeM));
        int col = static_cast<int>(std::floor(worldPos.x / kTileSizeM));
        return TileCoord{ row, col };
    }

    int slotIndexForTile(TileCoord coord)
    {
        auto wrap = [](int v) { return ((v % kStreamerGridDim) + kStreamerGridDim) % kStreamerGridDim; };
        int r = wrap(coord.row);
        int c = wrap(coord.col);
        return r * kStreamerGridDim + c;
    }


    TileCoord worldPosToAbsoluteTile(sf::Vector2f worldPos, TileCoord originTile)
    {
        TileCoord localTile = worldPosToTileCoord(worldPos);
        return TileCoord{
            originTile.row + localTile.row,
            originTile.col + localTile.col
        };
    }
}

namespace WorldCoordinates::Hex {

    sf::Vector2i worldToHex(float worldX, float worldZ)
    {
        float q = (2.f/3.f * worldX) / kHexSize;
        float r = (worldZ / (kHexSize * std::sqrt(3.f))) - q / 2.f;

        float s = -q - r;
        int rq = int(std::round(q));
        int rr = int(std::round(r));
        int rs = int(std::round(s));
        float dq = std::abs(rq - q);
        float dr = std::abs(rr - r);
        float ds = std::abs(rs - s);
        if (dq > dr && dq > ds)      rq = -rr - rs;
        else if (dr > ds)             rr = -rq - rs;

        return sf::Vector2i(rq, rr);
    }

    sf::Vector2f hexToWorld(int q, int r)
    {
        float x = kHexSize * 3.f/2.f * q;
        float z = kHexSize * std::sqrt(3.f) * (r + q/2.f);
        return sf::Vector2f(x, z);
    }

}