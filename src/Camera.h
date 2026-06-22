#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>
#include <vector>
#include <memory>

// forward declarations to avoid heavy includes in this header
struct CTransform3D;
struct CCamera;

namespace Camera
{
    sf::Vector3f worldToCamera(const sf::Vector3f& world, float pitch, float yaw, float roll);
    sf::Vector3f cameraToWorld(const sf::Vector3f& camera, float pitch, float yaw, float roll);
    bool worldToScreen(const CTransform3D& cameraTransform, const CCamera& cameraData, const sf::Vector3f& world, sf::Vector2f& screenOut);
    sf::Vector3f screenToWorld(const CTransform3D& cameraTransform, const CCamera& cameraData, sf::Vector2f screen);
    sf::Vector3f getForwardXZ(const CTransform3D& cameraTransform);
    sf::Vector3f getForward(const CTransform3D& cameraTransform);
    sf::Vector3f getRight(const CTransform3D& cameraTransform);
    sf::Vector3f getUp(const CTransform3D& cameraTransform);
    sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b);
    sf::Vector3f rotate(const sf::Vector3f& v, float pitch, float yaw, float roll); // legacy alias for worldToCamera
    sf::Vector3f rotateInverse(const sf::Vector3f& v, float pitch, float yaw, float roll); // legacy alias for cameraToWorld
    sf::Vector3f normalize(const sf::Vector3f& v);
    std::array<std::array<float, 3>, 3> getWorldToCamMatrix(float pitch, float yaw, float roll);
    std::array<float, 16> getVPMatrix(const CTransform3D& t, const CCamera& c);
    std::array<float, 16> getViewMatrix(const CTransform3D& t);
    std::array<float, 16> getProjectionMatrix(const CCamera& c);
}