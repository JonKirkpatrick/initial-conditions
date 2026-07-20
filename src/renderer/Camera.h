#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>
#include <vector>
#include <memory>

struct CTransform3D;
struct CCamera;

namespace Camera
{
    sf::Vector3f worldToCamera(const sf::Vector3f& world, const CTransform3D& transform);
    sf::Vector3f cameraToWorld(const sf::Vector3f& camera, const CTransform3D& transform);
    bool worldToScreen(const CTransform3D& cameraTransform, const CCamera& cameraData, const sf::Vector3f& world, sf::Vector2f& screenOut);
    sf::Vector3f screenToWorld(const CTransform3D& cameraTransform, const CCamera& cameraData, sf::Vector2f screen);
    sf::Vector3f getForwardXZ(const CTransform3D& cameraTransform);
    sf::Vector3f getForward(const CTransform3D& cameraTransform);
    sf::Vector3f getRight(const CTransform3D& cameraTransform);
    sf::Vector3f getUp(const CTransform3D& cameraTransform);
    sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b);
    sf::Vector3f rotate(const sf::Vector3f& v, const CTransform3D& t);
    sf::Vector3f rotateInverse(const sf::Vector3f& v, const CTransform3D& t);
    sf::Vector3f normalize(const sf::Vector3f& v);
    std::array<std::array<float, 3>, 3> getWorldToCamMatrix(const CTransform3D& t);
    std::array<float, 16> getVPMatrix(const CTransform3D& t, const CCamera& c);
    std::array<float, 16> getViewMatrix(const CTransform3D& t);
    std::array<float, 16> getProjectionMatrix(const CCamera& c);
    std::array<float, 16> getOrthoMatrix(float left, float right, float bottom, float top, float zNear, float zFar);
    void getFrustumBoundingSphere(const CTransform3D& camTransform, const CCamera& camData, 
                                  float splitNear, float splitFar, 
                                  sf::Vector3f& outCentroid, float& outRadius);
}