#include "Topography.h"
#include "Assets.h"
#include "WorldCoordinates.hpp"

namespace Topography {

    float heightAt(const TerrainContext& ctx, sf::Vector2f worldPos) {
        using namespace WorldCoordinates::Square;

        if (!ctx.streamer) {
            return 0.0f;
        }

        TileCoord originTile = ctx.streamer->getOriginTile();
        TileCoord absoluteTile = worldPosToAbsoluteTile(worldPos, originTile);

        const float* tileBuffer = ctx.streamer->getTileData(absoluteTile);
        if (!tileBuffer) {
            return 0.0f; 
        }

        constexpr float kTileSizeM = kTexelSizeM * kTileResolution;
        int localColIdx = absoluteTile.col - originTile.col;
        int localRowIdx = absoluteTile.row - originTile.row;

        float tileMinX = localColIdx * kTileSizeM;
        float tileMinZ = localRowIdx * kTileSizeM;

        float localX = (worldPos.x - tileMinX) / kTexelSizeM;
        float localZ = (worldPos.y - tileMinZ) / kTexelSizeM;

        localX = std::clamp(localX, 0.0f, static_cast<float>(kTileResolution - 1));
        localZ = std::clamp(localZ, 0.0f, static_cast<float>(kTileResolution - 1));

        int tx0 = static_cast<int>(localX);
        int tz0 = static_cast<int>(localZ);

        float fx = localX - static_cast<float>(tx0);
        float fz = localZ - static_cast<float>(tz0);

        int bX0 = tx0;
        int bZ0 = tz0;

        constexpr int kStride = kTileResolution + kApronTexels; // 257
        
        bX0 = std::clamp(bX0, 0, kTileResolution - 1); 
        bZ0 = std::clamp(bZ0, 0, kTileResolution - 1);

        size_t idx00 = static_cast<size_t>(bZ0) * kStride + bX0;
        size_t idx10 = idx00 + 1;
        size_t idx01 = idx00 + kStride;
        size_t idx11 = idx01 + 1;

        float h00 = tileBuffer[idx00];
        float h10 = tileBuffer[idx10];
        float h01 = tileBuffer[idx01];
        float h11 = tileBuffer[idx11];

        return (h00 + fx * (h10 - h00)) * (1.0f - fz) + (h01 + fx * (h11 - h01)) * fz;
    }

    sf::Vector3f normalAt(const TerrainContext& ctx, sf::Vector2f worldPos) {
        using namespace WorldCoordinates::Square;

        if (!ctx.streamer) {
            return sf::Vector3f(0.0f, 1.0f, 0.0f);
        }

        TileCoord originTile = ctx.streamer->getOriginTile();
        TileCoord absoluteTile = worldPosToAbsoluteTile(worldPos, originTile);

        const float* tileBuffer = ctx.streamer->getTileData(absoluteTile);
        if (!tileBuffer) {
            return sf::Vector3f(0.0f, 1.0f, 0.0f); 
        }

        constexpr float kTileSizeM = kTexelSizeM * kTileResolution;
        int localColIdx = absoluteTile.col - originTile.col;
        int localRowIdx = absoluteTile.row - originTile.row;

        float tileMinX = localColIdx * kTileSizeM;
        float tileMinZ = localRowIdx * kTileSizeM;

        float localX = (worldPos.x - tileMinX) / kTexelSizeM;
        float localZ = (worldPos.y - tileMinZ) / kTexelSizeM;

        localX = std::clamp(localX, 0.0f, static_cast<float>(kTileResolution - 1));
        localZ = std::clamp(localZ, 0.0f, static_cast<float>(kTileResolution - 1));

        int tx0 = static_cast<int>(localX);
        int tz0 = static_cast<int>(localZ);

        float fx = localX - static_cast<float>(tx0);
        float fz = localZ - static_cast<float>(tz0);

        int bX0 = tx0;
        int bZ0 = tz0;

        constexpr int kStride = kTileResolution + kApronTexels; // 257
        bX0 = std::clamp(bX0, 0, kTileResolution - 1); 
        bZ0 = std::clamp(bZ0, 0, kTileResolution - 1);

        size_t idx00 = static_cast<size_t>(bZ0) * kStride + bX0;
        size_t idx10 = idx00 + 1;
        size_t idx01 = idx00 + kStride;
        size_t idx11 = idx01 + 1;

        float h00 = tileBuffer[idx00];
        float h10 = tileBuffer[idx10];
        float h01 = tileBuffer[idx01];
        float h11 = tileBuffer[idx11];

        float dHdx = (1.0f - fz) * (h10 - h00) + fz * (h11 - h01);
        float dHdz = (1.0f - fx) * (h01 - h00) + fx * (h11 - h10);

        sf::Vector3f normal(-dHdx / kTexelSizeM, 1.0f, -dHdz / kTexelSizeM);

        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0f) {
            normal.x /= length; normal.y /= length; normal.z /= length;
        }
        return normal;
    }
}