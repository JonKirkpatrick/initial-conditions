#include "renderer/Camera.h"
#include "ecs/ComponentTypes.hpp"
#include <cmath>

namespace Camera {

    sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b) {
        return sf::Vector3f(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    sf::Vector3f cameraToWorld(const sf::Vector3f& v, const CTransform3D& t) {
        // Local to World = Rotate vector forward by the orientation quaternion
        glm::vec3 localVec(v.x, v.y, v.z);
        glm::vec3 worldVec = t.orientation() * localVec;
        
        return sf::Vector3f(worldVec.x, worldVec.y, worldVec.z);
    }

    sf::Vector3f worldToCamera(const sf::Vector3f& v, const CTransform3D& t) {
        // World to Local = Rotate vector by the INVERSE (conjugate) of the orientation quaternion
        glm::vec3 worldVec(v.x, v.y, v.z);
        
        // glm::conjugate(q) is an incredibly fast O(1) operation for unit quaternions—it just negates the vec3 part!
        glm::vec3 localVec = glm::conjugate(t.orientation()) * worldVec;
        
        return sf::Vector3f(localVec.x, localVec.y, localVec.z);
    }

    bool worldToScreen(const CTransform3D& t, const CCamera& c, const sf::Vector3f& world, sf::Vector2f& screenOut) {
        sf::Vector3f rel = world - t.pos;
        sf::Vector3f cam = worldToCamera(rel, t);
        
        // In right-handed camera space, objects in front of the lens have a NEGATIVE Z.
        // If cam.z is greater than -nearPlane (e.g., -0.05), it's either too close or behind the camera.
        if (cam.z > -c.nearPlane) return false;

        float f = 1.0f / std::tan(c.fovY * 0.5f);
        
        // Perspective division: dividing by the positive distance forward (-cam.z)
        float w_divisor = -cam.z; 
        float x_ndc = (cam.x * f / c.aspectRatio) / w_divisor;
        float y_ndc = (cam.y * f) / w_divisor;
        
        // Convert from NDC [-1, 1] to screen coordinate pixels
        screenOut.x = (x_ndc * 0.5f + 0.5f) * static_cast<float>(c.viewportSize.x);
        screenOut.y = (1.0f - (y_ndc * 0.5f + 0.5f)) * static_cast<float>(c.viewportSize.y);
        return true;
    }

    sf::Vector3f screenToWorld(const CTransform3D& t, const CCamera& c, sf::Vector2f screen) {
        // 1. Convert screen pixels back into Normalized Device Coordinates [-1, 1]
        float x_ndc = (screen.x / static_cast<float>(c.viewportSize.x)) * 2.f - 1.f;
        float y_ndc = 1.f - (screen.y / static_cast<float>(c.viewportSize.y)) * 2.f;
        
        float f = std::tan(c.fovY * 0.5f);
        
        // 2. Formulate the ray direction pointing down the negative Z axis in local space
        sf::Vector3f rayDirLocal(x_ndc * f * c.aspectRatio, y_ndc * f, -1.f);
        
        // 3. Transform the local ray direction into absolute world space vectors
        sf::Vector3f rayDirWorld = cameraToWorld(rayDirLocal, t);
        
        // 4. Trace down to intersect with the flat landscape floor (Y = 0)
        if (std::abs(rayDirWorld.y) < 1e-6f) return t.pos;
        
        float tt = -t.pos.y / rayDirWorld.y;
        if (tt < 0.f) return t.pos; // Intersection is behind the camera eye
        
        return t.pos + rayDirWorld * tt;
    }

    sf::Vector3f getForward(const CTransform3D& t) {
        return t.forward();
    }

    sf::Vector3f getRight(const CTransform3D& t) {
        return t.right();
    }

    sf::Vector3f getUp(const CTransform3D& t) {
        return t.up();
    }

    sf::Vector3f rotate(const sf::Vector3f& v, const CTransform3D& t) {
        return worldToCamera(v, t);
    }

    sf::Vector3f rotateInverse(const sf::Vector3f& v, const CTransform3D& t) {
        return cameraToWorld(v, t);
    }

    sf::Vector3f normalize(const sf::Vector3f& v) {
        float m = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if (m <= 1e-6f) return v;
        return v / m;
    }

    std::array<std::array<float, 3>, 3> getWorldToCamMatrix(const CTransform3D& t) {
        std::array<std::array<float,3>,3> m{};
        // Read directly from the cached transform vectors!
        // No cos/sin calls, completely rigid
        m[0] = { t.right().x,   t.up().x,   -t.forward().x };
        m[1] = { t.right().y,   t.up().y,   -t.forward().y };
        m[2] = { t.right().z,   t.up().z,   -t.forward().z };
        return m;
    }

    std::array<float, 16> getViewMatrix(const CTransform3D& t) {
        // Get rotation matrix from world vectors -> camera space
        auto rot = getWorldToCamMatrix(t);

        // Compute translation in camera space relative to current position
        double tx = -(double(rot[0][0]) * t.pos.x + double(rot[1][0]) * t.pos.y + double(rot[2][0]) * t.pos.z);
        double ty = -(double(rot[0][1]) * t.pos.x + double(rot[1][1]) * t.pos.y + double(rot[2][1]) * t.pos.z);
        double tz = -(double(rot[0][2]) * t.pos.x + double(rot[1][2]) * t.pos.y + double(rot[2][2]) * t.pos.z);

        // Standalone View matrix (4x4 column-major)
        return {
            // Column 0
            rot[0][0], rot[0][1], rot[0][2], 0.f,
            // Column 1
            rot[1][0], rot[1][1], rot[1][2], 0.f,
            // Column 2
            rot[2][0], rot[2][1], rot[2][2], 0.f,
            // Column 3 (camera space translation)
            float(tx),        float(ty),        float(tz),        1.f
        };
    }

    std::array<float, 16> getProjectionMatrix(const CCamera& c) {
        float f     = 1.0f / std::tan(c.fovY * 0.5f);
        float zNear = c.nearPlane;
        float zFar  = c.farPlane;
        
        // Zero-to-one standard right-handed mapping
        float zRange = zNear - zFar; 

        // Standalone Perspective Projection matrix (4x4 column-major)
        return {
            f / c.aspectRatio, 0.f,  0.f,  0.f,
            0.f,               f,  0.f,  0.f,
            0.f,               0.f,  zFar / zRange,  -1.f,
            0.f,               0.f,  (zNear * zFar) / zRange, 0.f
        };
    }

    std::array<float, 16> getVPMatrix(const CTransform3D& t, const CCamera& c)
    {
        // 1. Get rotation: world -> camera (columns = transformed basis vectors)
        auto rot = getWorldToCamMatrix(t);

        // Translation in camera space: t_cam = - (R * world_pos)
        double tx = -(double(rot[0][0]) * t.pos.x + double(rot[1][0]) * t.pos.y + double(rot[2][0]) * t.pos.z);
        double ty = -(double(rot[0][1]) * t.pos.x + double(rot[1][1]) * t.pos.y + double(rot[2][1]) * t.pos.z);
        double tz = -(double(rot[0][2]) * t.pos.x + double(rot[1][2]) * t.pos.y + double(rot[2][2]) * t.pos.z);

        // View matrix (column-major)
        std::array<float, 16> V = {
            // Column 0
            rot[0][0], rot[0][1], rot[0][2], 0.f,
            // Column 1
            rot[1][0], rot[1][1], rot[1][2], 0.f,
            // Column 2
            rot[2][0], rot[2][1], rot[2][2], 0.f,
            // Column 3 (translation)
            float(tx),        float(ty),        float(tz),        1.f
        };

        // 2. Updated Projection matrix block to utilize the corrected [0, 1] depth logic
        float f     = 1.0f / std::tan(c.fovY * 0.5f);
        float zNear = c.nearPlane;
        float zFar  = c.farPlane;
        float zRange = zNear - zFar;

        std::array<float, 16> P = {
            // Column 0
            f / c.aspectRatio, 0.f,  0.f,  0.f,
            // Column 1
            0.f,               f,  0.f,  0.f,
            // Column 2
            0.f,               0.f,  zFar / zRange,  -1.f, // Correct mapping for native right-handed depth
            // Column 3
            0.f,               0.f,  (zNear * zFar) / zRange, 0.f
        };

        // 3. P * V Multiplication (remains structurally pristine)
        std::array<float, 16> VP = {};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                for (int k = 0; k < 4; ++k) {
                    VP[col*4 + row] += P[k*4 + row] * V[col*4 + k];
                }
            }
        }

        return VP;
    }
} // namespace Camera
