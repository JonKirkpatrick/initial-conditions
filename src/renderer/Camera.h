#pragma once

/**
 * @file Camera.h
 * @brief Camera transformations, projection matrix routines, and 3D coordinate mapping.
 * 
 * Provides vector and matrix transformation functions for converting between World, 
 * Camera, and Screen spaces, computing view/projection matrices, directional basis vectors, 
 * and frustum bounding spheres for view culling and cascading shadow maps.
 */

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>
#include <vector>
#include <memory>

struct CTransform3D;
struct CCamera;

namespace Camera
{
    /**
     * @brief Transforms a point from 3D world space into camera local space.
     * @param world 3D world space position.
     * @param transform 3D transform of the camera entity.
     * @return 3D position relative to the camera frame.
     */
    sf::Vector3f worldToCamera(const sf::Vector3f& world, const CTransform3D& transform);

    /**
     * @brief Transforms a point from camera local space back into 3D world space.
     * @param camera 3D camera local position.
     * @param transform 3D transform of the camera entity.
     * @return 3D world space position.
     */
    sf::Vector3f cameraToWorld(const sf::Vector3f& camera, const CTransform3D& transform);

    /**
     * @brief Projects a 3D world space point to 2D screen space coordinates.
     * @param cameraTransform 3D transform of the camera entity.
     * @param cameraData Camera projection parameters (FOV, aspect ratio, clip planes).
     * @param world 3D world space target position.
     * @param[out] screenOut Receives projected 2D screen pixel coordinates.
     * @return `true` if the point is in front of the camera, `false` if culled behind the camera.
     */
    bool worldToScreen(const CTransform3D& cameraTransform, const CCamera& cameraData, const sf::Vector3f& world, sf::Vector2f& screenOut);

    /**
     * @brief Unprojects a 2D screen pixel location back into a 3D world space position.
     * @param cameraTransform 3D transform of the camera entity.
     * @param cameraData Camera projection parameters.
     * @param screen 2D screen pixel coordinates.
     * @return Reconstructed 3D world position (typically intersecting the target plane).
     */
    sf::Vector3f screenToWorld(const CTransform3D& cameraTransform, const CCamera& cameraData, sf::Vector2f screen);

    /**
     * @brief Gets the normalized forward vector projected onto the horizontal XZ plane.
     * @param cameraTransform 3D transform of the camera entity.
     * @return Normalized 3D direction vector with Y forced to 0.0f.
     */
    sf::Vector3f getForwardXZ(const CTransform3D& cameraTransform);

    /**
     * @brief Gets the camera's true forward direction vector in 3D world space.
     * @param cameraTransform 3D transform of the camera entity.
     * @return Normalized 3D forward direction vector.
     */
    sf::Vector3f getForward(const CTransform3D& cameraTransform);

    /**
     * @brief Gets the camera's right direction vector in 3D world space.
     * @param cameraTransform 3D transform of the camera entity.
     * @return Normalized 3D right direction vector.
     */
    sf::Vector3f getRight(const CTransform3D& cameraTransform);

    /**
     * @brief Gets the camera's up direction vector in 3D world space.
     * @param cameraTransform 3D transform of the camera entity.
     * @return Normalized 3D up direction vector.
     */
    sf::Vector3f getUp(const CTransform3D& cameraTransform);

    /**
     * @brief Computes the cross product of two 3D vectors.
     * @param a First vector.
     * @param b Second vector.
     * @return Resulting perpendicular 3D vector.
     */
    sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b);

    /**
     * @brief Rotates a vector by the orientation defined in a 3D transform.
     * @param v Target vector.
     * @param t 3D transform containing rotation matrix/quaternion.
     * @return Rotated 3D vector.
     */
    sf::Vector3f rotate(const sf::Vector3f& v, const CTransform3D& t);

    /**
     * @brief Rotates a vector by the inverse orientation defined in a 3D transform.
     * @param v Target vector.
     * @param t 3D transform.
     * @return Inversely rotated 3D vector.
     */
    sf::Vector3f rotateInverse(const sf::Vector3f& v, const CTransform3D& t);

    /**
     * @brief Normalizes a 3D vector to unit length.
     * @param v Input 3D vector.
     * @return Unit-length 3D vector.
     */
    sf::Vector3f normalize(const sf::Vector3f& v);

    /**
     * @brief Generates a 3x3 rotation matrix for world-to-camera coordinate transformation.
     * @param t 3D camera transform.
     * @return 3x3 array containing row-major rotation values.
     */
    std::array<std::array<float, 3>, 3> getWorldToCamMatrix(const CTransform3D& t);

    /**
     * @brief Generates a combined 4x4 View-Projection (VP) matrix.
     * @param t 3D camera transform.
     * @param c Camera projection component.
     * @return Flat column-major array of 16 float matrix elements.
     */
    std::array<float, 16> getVPMatrix(const CTransform3D& t, const CCamera& c);

    /**
     * @brief Generates a 4x4 View matrix from camera position and orientation.
     * @param t 3D camera transform.
     * @return Flat column-major array of 16 float matrix elements.
     */
    std::array<float, 16> getViewMatrix(const CTransform3D& t);

    /**
     * @brief Generates a 4x4 perspective Projection matrix from camera parameters.
     * @param c Camera projection component.
     * @return Flat column-major array of 16 float matrix elements.
     */
    std::array<float, 16> getProjectionMatrix(const CCamera& c);

    /**
     * @brief Generates a 4x4 Orthographic Projection matrix.
     * @param left Left plane clipping bound.
     * @param right Right plane clipping bound.
     * @param bottom Bottom plane clipping bound.
     * @param top Top plane clipping bound.
     * @param zNear Near clipping plane distance.
     * @param zFar Far clipping plane distance.
     * @return Flat column-major array of 16 float matrix elements.
     */
    std::array<float, 16> getOrthoMatrix(float left, float right, float bottom, float top, float zNear, float zFar);

    /**
     * @brief Computes a minimal bounding sphere enclosing a sub-frustum slice.
     * 
     * Useful for constructing tight bounding volumes for cascading shadow mapping (CSM) 
     * and view-frustum culling.
     * 
     * @param camTransform 3D camera transform.
     * @param camData Camera projection parameters.
     * @param splitNear Near boundary distance of the frustum split segment.
     * @param splitFar Far boundary distance of the frustum split segment.
     * @param[out] outCentroid Receives calculated 3D world centroid position of bounding sphere.
     * @param[out] outRadius Receives calculated bounding sphere radius.
     */
    void getFrustumBoundingSphere(const CTransform3D& camTransform, const CCamera& camData, 
                                  float splitNear, float splitFar, 
                                  sf::Vector3f& outCentroid, float& outRadius);
}