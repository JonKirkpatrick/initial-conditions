#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>
#include <vector>
#include <tuple>

namespace Camera
{    
    sf::Vector3f worldToCamera(const sf::Vector3f& world, float pitch, float yaw, float roll);
    sf::Vector3f cameraToWorld(const sf::Vector3f& camera, float pitch, float yaw, float roll);
    bool worldToScreen(std::shared_ptr<Entity> cameraEntity, const sf::Vector3f& world, sf::Vector2f& screenOut);
    sf::Vector3f screenToWorld(std::shared_ptr<Entity> cameraEntity, sf::Vector2f screen);
    std::tuple<sf::Vector2f, sf::Vector2f, bool> horizonScreenLine(std::shared_ptr<Entity> cameraEntity);
    bool groundPolygon(std::shared_ptr<Entity> cameraEntity, std::vector<sf::Vector2f>& points);
    sf::Vector3f getForwardXZ(std::shared_ptr<Entity> cameraEntity);
    sf::Vector3f getForward(std::shared_ptr<Entity> cameraEntity);
    sf::Vector3f rotate(const sf::Vector3f& v, float pitch, float yaw, float roll); // legacy alias for worldToCamera
    sf::Vector3f rotateInverse(const sf::Vector3f& v, float pitch, float yaw, float roll); // legacy alias for cameraToWorld
    sf::Vector3f normalize(const sf::Vector3f& v);
    // Compute inverse rotation matrix once per frame (cheaper than per-pixel trig)
    std::array<std::array<float, 3>, 3> getInverseRotationMatrix(float pitch, float yaw, float roll);
}