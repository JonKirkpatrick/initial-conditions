#pragma once

#include <SFML/System.hpp>
#include <array>
#include <cmath>

namespace Topography {

    // ============================================================================
    // Terrain Layer Definition
    // ============================================================================

    struct TerrainLayer
    {
        sf::Vector2f center;
        float radius;
        float falloffWidth;
        float topoHeight;
    };

    // ============================================================================
    // Terrain Query Functions
    // ============================================================================

    /// Mask calculation in C++ matching GLSL `maskFromD` (user-specified formulas).
    static inline float maskFromD_Cpp(float d, float rd, float falloff) {
        const float k = 1e-10f;
        float t = falloff;

        float s = 0.0f;
        float denom_s = d - k;
        if (std::fabs(denom_s) >= k) {
            s = (1.0f - std::fabs(d) / denom_s) * 0.5f;
        }

        float u = t - std::fabs(d - t);

        float g = 0.0f;
        float denom_g = std::fabs(u) - k;
        if (std::fabs(denom_g) >= k) {
            g = ((u / denom_g) + 1.0f) * 0.5f;
        }

        const float PI = 3.14159265358979323846f;
        float cosTerm = std::cos(PI * (d / (2.0f * t)));
        float b = g * ((cosTerm + 1.0f) * 0.5f);

        float m = s + b;
        if (m < 0.0f) m = 0.0f;
        if (m > 1.0f) m = 1.0f;

        return m;
    }

    /// Evaluate terrain height at a specific location given all terrain layers and active mask.
    inline float heightAt(float x, float z, const std::array<TerrainLayer, 16>& layers, uint32_t activeLayerMask) {
        float height = 0.0f;
        for (int i = 0; i < 16; ++i) {
            if ((activeLayerMask & (1u << i)) == 0) continue;

            const TerrainLayer& layer = layers[i];
            float dx   = x - layer.center.x;
            float dz   = z - layer.center.y;
            float dist = std::sqrt(dx * dx + dz * dz);

            float d = dist - layer.radius;
            float m = maskFromD_Cpp(d, layer.radius, layer.falloffWidth);
            if (m <= 0.0f) continue;

            height += m * layer.topoHeight;
        }
        return height;
    }

    /// Height contribution from a single layer.
    inline float evaluateLayerHeightAt(const TerrainLayer& layer, float x, float z) {
        float dx   = x - layer.center.x;
        float dz   = z - layer.center.y;
        float dist = std::sqrt(dx * dx + dz * dz);

        float d = dist - layer.radius;
        float m = maskFromD_Cpp(d, layer.radius, layer.falloffWidth);

        return layer.topoHeight * m;
    }

    /// Camera height above ground.
    inline float getCameraHeightAboveGround(const sf::Vector3f& camPos, const std::array<TerrainLayer, 16>& layers, uint32_t activeLayerMask) {
        float groundHeight = heightAt(camPos.x, camPos.z, layers, activeLayerMask);
        return camPos.y - groundHeight;
    }

    /// Compute the maximum height reached at any layer center.
    inline float computeSceneMaxHeight(const std::array<TerrainLayer, 16>& layers, uint32_t activeLayerMask) {
        float maxHeight = 0.0f;
        for (const auto& layer : layers) {
            maxHeight = std::max(maxHeight, heightAt(layer.center.x, layer.center.y, layers, activeLayerMask));
        }
        return std::max(1.0f, maxHeight * 1.5f);
    }

    /// Compute which layers should be active based on camera distance (LOD culling).
    inline uint32_t computeActiveLayerMask(const sf::Vector3f& cameraPos, const std::array<TerrainLayer, 16>& layers) {
        const float LAYER_CULL_DIST = 150000.0f;
        uint32_t mask = 0;
        
        for (int i = 0; i < 16; ++i) {
            float dx = cameraPos.x - layers[i].center.x;
            float dz = cameraPos.z - layers[i].center.y;
            float distSq = dx * dx + dz * dz;
            float cullRadiusSq = (LAYER_CULL_DIST + layers[i].radius) * 
                                 (LAYER_CULL_DIST + layers[i].radius);
            if (distSq <= cullRadiusSq) {
                mask |= (1u << i);
            }
        }
        return mask;
    }

} // namespace Topography
