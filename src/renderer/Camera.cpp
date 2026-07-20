#include "renderer/Camera.h"
#include "ecs/ComponentTypes.hpp"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace Camera {

    sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b) {
        return sf::Vector3f(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    sf::Vector3f cameraToWorld(const sf::Vector3f& v, const CTransform3D& t) {
        glm::vec3 localVec(v.x, v.y, v.z);
        glm::vec3 worldVec = t.orientation() * localVec;
        
        return sf::Vector3f(worldVec.x, worldVec.y, worldVec.z);
    }

    sf::Vector3f worldToCamera(const sf::Vector3f& v, const CTransform3D& t) {
        glm::vec3 worldVec(v.x, v.y, v.z);
        
        glm::vec3 localVec = glm::conjugate(t.orientation()) * worldVec;
        
        return sf::Vector3f(localVec.x, localVec.y, localVec.z);
    }

    bool worldToScreen(const CTransform3D& t, const CCamera& c, const sf::Vector3f& world, sf::Vector2f& screenOut) {
        sf::Vector3f rel = world - t.pos;
        sf::Vector3f cam = worldToCamera(rel, t);
        
        if (cam.z > -c.nearPlane) return false;

        float f = 1.0f / std::tan(c.fovY * 0.5f);
        
        float w_divisor = -cam.z; 
        float x_ndc = (cam.x * f / c.aspectRatio) / w_divisor;
        float y_ndc = (cam.y * f) / w_divisor;
        
        screenOut.x = (x_ndc * 0.5f + 0.5f) * static_cast<float>(c.viewportSize.x);
        screenOut.y = (1.0f - (y_ndc * 0.5f + 0.5f)) * static_cast<float>(c.viewportSize.y);
        return true;
    }

    sf::Vector3f screenToWorld(const CTransform3D& t, const CCamera& c, sf::Vector2f screen) {
        float x_ndc = (screen.x / static_cast<float>(c.viewportSize.x)) * 2.f - 1.f;
        float y_ndc = 1.f - (screen.y / static_cast<float>(c.viewportSize.y)) * 2.f;
        
        float f = std::tan(c.fovY * 0.5f);
        
        sf::Vector3f rayDirLocal(x_ndc * f * c.aspectRatio, y_ndc * f, -1.f);
        
        sf::Vector3f rayDirWorld = cameraToWorld(rayDirLocal, t);
        
        if (std::abs(rayDirWorld.y) < 1e-6f) return t.pos;
        
        float tt = -t.pos.y / rayDirWorld.y;
        if (tt < 0.f) return t.pos;
        
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
        m[0] = { t.right().x,   t.up().x,   -t.forward().x };
        m[1] = { t.right().y,   t.up().y,   -t.forward().y };
        m[2] = { t.right().z,   t.up().z,   -t.forward().z };
        return m;
    }

    std::array<float, 16> getViewMatrix(const CTransform3D& t) {
        auto rot = getWorldToCamMatrix(t);

        double tx = -(double(rot[0][0]) * t.pos.x + double(rot[1][0]) * t.pos.y + double(rot[2][0]) * t.pos.z);
        double ty = -(double(rot[0][1]) * t.pos.x + double(rot[1][1]) * t.pos.y + double(rot[2][1]) * t.pos.z);
        double tz = -(double(rot[0][2]) * t.pos.x + double(rot[1][2]) * t.pos.y + double(rot[2][2]) * t.pos.z);

        return {
            rot[0][0], rot[0][1], rot[0][2], 0.f,
            rot[1][0], rot[1][1], rot[1][2], 0.f,
            rot[2][0], rot[2][1], rot[2][2], 0.f,
            float(tx),        float(ty),        float(tz),        1.f
        };
    }

    std::array<float, 16> getProjectionMatrix(const CCamera& c) {
        float f     = 1.0f / std::tan(c.fovY * 0.5f);
        float zNear = c.nearPlane;
        float zFar  = c.farPlane;
        
        float zRange = zNear - zFar; 

        return {
            f / c.aspectRatio, 0.f,  0.f,  0.f,
            0.f,               f,  0.f,  0.f,
            0.f,               0.f,  zFar / zRange,  -1.f,
            0.f,               0.f,  (zNear * zFar) / zRange, 0.f
        };
    }

    std::array<float, 16> getVPMatrix(const CTransform3D& t, const CCamera& c)
    {
        auto rot = getWorldToCamMatrix(t);

        double tx = -(double(rot[0][0]) * t.pos.x + double(rot[1][0]) * t.pos.y + double(rot[2][0]) * t.pos.z);
        double ty = -(double(rot[0][1]) * t.pos.x + double(rot[1][1]) * t.pos.y + double(rot[2][1]) * t.pos.z);
        double tz = -(double(rot[0][2]) * t.pos.x + double(rot[1][2]) * t.pos.y + double(rot[2][2]) * t.pos.z);

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

        float f     = 1.0f / std::tan(c.fovY * 0.5f);
        float zNear = c.nearPlane;
        float zFar  = c.farPlane;
        float zRange = zNear - zFar;

        std::array<float, 16> P = {
            f / c.aspectRatio, 0.f,  0.f,  0.f,
            0.f,               f,  0.f,  0.f,
            0.f,               0.f,  zFar / zRange,  -1.f,
            0.f,               0.f,  (zNear * zFar) / zRange, 0.f
        };

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

    std::array<float, 16> getOrthoMatrix(float left, float right, float bottom, float top, float zNear, float zFar)
    {
        float xRange = right - left;
        float yRange = top - bottom;
        float zRange = zNear - zFar;

        return {
            2.0f / xRange, 0.f,           0.f,           0.f,
            0.f,           2.0f / yRange, 0.f,           0.f,
            0.f,           0.f,           1.0f / zRange, 0.f,
            -(right + left) / xRange, -(top + bottom) / yRange, zNear / zRange, 1.f
        };
    }

    void getFrustumBoundingSphere(const CTransform3D& camTransform, const CCamera& camData, 
                                  float splitNear, float splitFar, 
                                  sf::Vector3f& outCentroid, float& outRadius)
    {
        // 1. Reconstruct temporary sub-frustum specs
        CCamera subCam = camData;
        subCam.nearPlane = splitNear;
        subCam.farPlane  = splitFar;

        // 2. Derive the Inverse View-Projection Matrix using your pipeline
        auto vpRaw = getVPMatrix(camTransform, subCam);
        glm::mat4 camVP = glm::make_mat4(vpRaw.data());
        glm::mat4 camInvVP = glm::inverse(camVP);

        // 3. Define the standard Normalized Device Coordinates (NDC) cuboid edges
        static const glm::vec4 ndcCorners[8] = {
            {-1.f,-1.f, 0.f, 1.f}, { 1.f,-1.f, 0.f, 1.f}, { 1.f, 1.f, 0.f, 1.f}, {-1.f, 1.f, 0.f, 1.f},
            {-1.f,-1.f, 1.f, 1.f}, { 1.f,-1.f, 1.f, 1.f}, { 1.f, 1.f, 1.f, 1.f}, {-1.f, 1.f, 1.f, 1.f},
        };

        // 4. Back-project corners into absolute World Coordinates
        glm::vec3 worldCorners[8];
        glm::vec3 center(0.0f);

        for (int i = 0; i < 8; ++i) {
            glm::vec4 p = camInvVP * ndcCorners[i];
            worldCorners[i] = glm::vec3(p) / p.w;
            center += worldCorners[i];
        }
        center /= 8.0f;
        outCentroid = sf::Vector3f(center.x, center.y, center.z);

        // 5. Establish the absolute maximum boundary radius wrapping the frustum shell
        outRadius = 0.0f;
        for (int i = 0; i < 8; ++i) {
            float dist = glm::length(worldCorners[i] - center);
            outRadius = std::max(outRadius, dist);
        }
    }
} // namespace Camera
