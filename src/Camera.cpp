// #pragma once

#include "Entity.hpp"
#include "Camera.h"
#include "EntityManager.hpp"
#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>

namespace Camera
{
    sf::Vector3f rotate(const sf::Vector3f& v, float pitch, float yaw, float roll) {
        float cp = std::cos(-pitch), sp = std::sin(-pitch);
        float cy = std::cos(-yaw),   sy = std::sin(-yaw);
        float cr = std::cos(-roll),  sr = std::sin(-roll);

        // Yaw (Y)
        float x1 = cy * v.x + sy * v.z;
        float z1 = -sy * v.x + cy * v.z;
        float y1 = v.y;

        // Pitch (X)
        float y2 = cp * y1 - sp * z1;
        float z2 = sp * y1 + cp * z1;
        float x2 = x1;

        // Roll (Z)
        float x3 = cr * x2 - sr * y2;
        float y3 = sr * x2 + cr * y2;
        return sf::Vector3f(x3, y3, z2);
    }

    sf::Vector3f rotateInverse(const sf::Vector3f& v, float pitch, float yaw, float roll) {
        float cp = std::cos(-pitch), sp = std::sin(-pitch);
        float cy = std::cos(-yaw),   sy = std::sin(-yaw);
        float cr = std::cos(-roll),  sr = std::sin(-roll);
        
        // Undo roll first
        float x1 = cr * v.x + sr * v.y;
        float y1 = -sr * v.x + cr * v.y;
        float z1 = v.z;
        
        // Undo pitch
        float y2 = cp * y1 + sp * z1;
        float z2 = -sp * y1 + cp * z1;
        float x2 = x1;
        
        // Undo yaw last
        float x3 = cy * x2 - sy * z2;
        float z3 = sy * x2 + cy * z2;
        float y3 = y2;
        
        return sf::Vector3f(x3, y3, z3);
    }

    sf::Vector3f normalize(const sf::Vector3f& v)
    {
        float lenSq = v.x * v.x +
                      v.y * v.y +
                      v.z * v.z;

        if (lenSq == 0.f)
            return sf::Vector3f(0.f, 0.f, 0.f);

        float invLen = 1.f / std::sqrt(lenSq);

        return sf::Vector3f(
            v.x * invLen,
            v.y * invLen,
            v.z * invLen
        );
    }

    std::array<std::array<float, 3>, 3> getInverseRotationMatrix(float pitch, float yaw, float roll)
    {
        const sf::Vector3f basisX = rotateInverse(sf::Vector3f(1.f, 0.f, 0.f), pitch, yaw, roll);
        const sf::Vector3f basisY = rotateInverse(sf::Vector3f(0.f, 1.f, 0.f), pitch, yaw, roll);
        const sf::Vector3f basisZ = rotateInverse(sf::Vector3f(0.f, 0.f, 1.f), pitch, yaw, roll);

        return std::array<std::array<float, 3>, 3>{
            std::array<float, 3>{basisX.x, basisY.x, basisZ.x},
            std::array<float, 3>{basisX.y, basisY.y, basisZ.y},
            std::array<float, 3>{basisX.z, basisY.z, basisZ.z}
        };
    }
    
    sf::Vector3f getForwardXZ(std::shared_ptr<Entity> cameraEntity) {
        auto cameraTransform = cameraEntity->get<CTransform3D>();
        auto forward = rotateInverse(sf::Vector3f(0, 0, -1), cameraTransform.pitch, cameraTransform.yaw, cameraTransform.roll);
        forward.y = 0.f; // Flatten to XZ plane
        float forwardLen = std::max(std::sqrt(forward.x * forward.x + forward.z * forward.z), 0.0001f);
        forward /= forwardLen; // Normalize
        return forward;
    }

    sf::Vector3f getForward(std::shared_ptr<Entity> cameraEntity) {
        auto cameraTransform = cameraEntity->get<CTransform3D>();
        return rotateInverse(sf::Vector3f(0, 0, -1), cameraTransform.pitch, cameraTransform.yaw, cameraTransform.roll);
    }

    bool worldToScreen(std::shared_ptr<Entity> cameraEntity, const sf::Vector3f& world, sf::Vector2f& screenOut) {
        auto cameraTransform = cameraEntity->get<CTransform3D>();
        auto cameraData = cameraEntity->get<CCamera>();
        sf::Vector3f rel = world - cameraTransform.pos;
        sf::Vector3f cam = rotate(rel, cameraTransform.pitch, cameraTransform.yaw, cameraTransform.roll);
        if (cam.z >= -cameraData.nearPlane) {
            return false;
        } 
        float f = 1.0f / std::tan(cameraData.fovY * 0.5f);
        float x_ndc = (cam.x * f / cameraData.aspectRatio) / -cam.z;
        float y_ndc = (cam.y * f) / -cam.z;
        screenOut.x = (x_ndc * 0.5f + 0.5f) * cameraData.viewportSize.x;
        screenOut.y = (1.0f - (y_ndc * 0.5f + 0.5f)) * cameraData.viewportSize.y;
        return true;
    }

    sf::Vector3f screenToWorld(std::shared_ptr<Entity> cameraEntity, sf::Vector2f screen) {
        auto cameraTransform = cameraEntity->get<CTransform3D>();
        auto cameraData = cameraEntity->get<CCamera>();
        float x_ndc = (screen.x / cameraData.viewportSize.x) * 2.f - 1.f;
        float y_ndc = 1.f - (screen.y / cameraData.viewportSize.y) * 2.f;
        float f = std::tan(cameraData.fovY * 0.5f);
        sf::Vector3f rayDir(x_ndc * f * cameraData.aspectRatio, y_ndc * f, -1.f);
        rayDir = rotateInverse(rayDir, cameraTransform.pitch, cameraTransform.yaw, cameraTransform.roll); // into world space
        // Intersect with y=0
        if (std::abs(rayDir.y) < 0.0001f) return cameraTransform.pos; // parallel to ground
        float t = -cameraTransform.pos.y / rayDir.y;
        if (t < 0.f) return cameraTransform.pos; // ground behind camera
        return cameraTransform.pos + rayDir * t;
    }
}
