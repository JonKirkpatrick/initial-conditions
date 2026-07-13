#include "Topography.h"
#include "Assets.h"

namespace Topography {

    // Direct call into the already bounds-checked HeightArray accessor —
    // no decode math, no maxHeight rescale, the buffer already holds true meters.
    inline float sampleHeight(const TerrainContext& ctx, int x, int y) {
        return ctx.heightArray->sample(x, y);
    }

    float heightAt(const TerrainContext& ctx, float worldX, float worldZ) {
        float u = std::clamp((worldX - ctx.worldMin.x) / ctx.worldSize.x, 0.0f, 1.0f);
        float v = std::clamp((worldZ - ctx.worldMin.y) / ctx.worldSize.y, 0.0f, 1.0f);

        float px = u * (ctx.heightArray->width - 1);
        float py = v * (ctx.heightArray->height - 1);

        int x0 = static_cast<int>(px);
        int y0 = static_cast<int>(py);
        int x1 = std::min(x0 + 1, ctx.heightArray->width - 1);
        int y1 = std::min(y0 + 1, ctx.heightArray->height - 1);

        float fx = px - x0;
        float fy = py - y0;

        float h00 = sampleHeight(ctx, x0, y0);
        float h10 = sampleHeight(ctx, x1, y0);
        float h01 = sampleHeight(ctx, x0, y1);
        float h11 = sampleHeight(ctx, x1, y1);

        return (h00 + fx * (h10 - h00)) * (1.0f - fy) + (h01 + fx * (h11 - h01)) * fy;
    }

    sf::Vector3f normalAt(const TerrainContext& ctx, float worldX, float worldZ) {
        float u = std::clamp((worldX - ctx.worldMin.x) / ctx.worldSize.x, 0.0f, 1.0f);
        float v = 1.0f - std::clamp((worldZ - ctx.worldMin.y) / ctx.worldSize.y, 0.0f, 1.0f);

        float px = u * (ctx.heightArray->width - 1);
        float py = v * (ctx.heightArray->height - 1);

        int x0 = static_cast<int>(px);
        int y0 = static_cast<int>(py);
        int x1 = std::min(x0 + 1, ctx.heightArray->width - 1);
        int y1 = std::min(y0 + 1, ctx.heightArray->height - 1);

        float fx = px - x0;
        float fy = py - y0;

        float h00 = sampleHeight(ctx, x0, y0);
        float h10 = sampleHeight(ctx, x1, y0);
        float h01 = sampleHeight(ctx, x0, y1);
        float h11 = sampleHeight(ctx, x1, y1);

        float dHdx = (1.0f - fy) * (h10 - h00) + fy * (h11 - h01);
        float dHdz = (1.0f - fx) * (h01 - h00) + fx * (h11 - h10);

        float texelSizeX = ctx.worldSize.x / (ctx.heightArray->width - 1);
        float texelSizeZ = ctx.worldSize.y / (ctx.heightArray->height - 1);

        sf::Vector3f normal(-dHdx / texelSizeX, 1.0f, dHdz / texelSizeZ);

        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0f) {
            normal.x /= length; normal.y /= length; normal.z /= length;
        }
        return normal;
    }
}