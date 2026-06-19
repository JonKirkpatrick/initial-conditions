#include "Camera.h"
#include "ComponentTypes.hpp"
#include <cmath>

namespace Camera {

    sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b) {
        return sf::Vector3f(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    sf::Vector3f worldToCamera(const sf::Vector3f& v, float pitch, float yaw, float roll) {
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cy = std::cos(yaw),   sy = std::sin(yaw);
        float cr = std::cos(roll),  sr = std::sin(roll);

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

    sf::Vector3f cameraToWorld(const sf::Vector3f& v, float pitch, float yaw, float roll) {
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cy = std::cos(yaw),   sy = std::sin(yaw);
        float cr = std::cos(roll),  sr = std::sin(roll);
        
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

    bool worldToScreen(const CTransform3D& t, const CCamera& c, const sf::Vector3f& world, sf::Vector2f& screenOut) {
        sf::Vector3f rel = world - t.pos;
        sf::Vector3f cam = worldToCamera(rel, t.pitch, t.yaw, t.roll);
        if (cam.z > -c.nearPlane) return false;

        float f = 1.0f / std::tan(c.fovY * 0.5f);
        float x_ndc = (cam.x * f / c.aspectRatio) / -cam.z;
        float y_ndc = (cam.y * f) / -cam.z;
        screenOut.x = (x_ndc * 0.5f + 0.5f) * static_cast<float>(c.viewportSize.x);
        screenOut.y = (1.0f - (y_ndc * 0.5f + 0.5f)) * static_cast<float>(c.viewportSize.y);
        return true;
    }

    sf::Vector3f screenToWorld(const CTransform3D& t, const CCamera& c, sf::Vector2f screen) {
        float x_ndc = (screen.x / static_cast<float>(c.viewportSize.x)) * 2.f - 1.f;
        float y_ndc = 1.f - (screen.y / static_cast<float>(c.viewportSize.y)) * 2.f;
        float f = std::tan(c.fovY * 0.5f);
        sf::Vector3f rayDir(x_ndc * f * c.aspectRatio, y_ndc * f, -1.f);
        rayDir = cameraToWorld(rayDir, t.pitch, t.yaw, t.roll);
        if (std::abs(rayDir.y) < 1e-6f) return t.pos;
        float tt = -t.pos.y / rayDir.y;
        if (tt < 0.f) return t.pos;
        return t.pos + rayDir * tt;
    }

    sf::Vector3f getForwardXZ(const CTransform3D& t) {
        float cp = std::cos(t.pitch), sp = std::sin(t.pitch);
        float cy = std::cos(t.yaw),   sy = std::sin(t.yaw);
        return sf::Vector3f(-sy * cp, 0.f, -cy * cp);
    }

    sf::Vector3f getForward(const CTransform3D& t) {
        float cp = std::cos(t.pitch), sp = std::sin(t.pitch);
        float cy = std::cos(t.yaw),   sy = std::sin(t.yaw);
        return sf::Vector3f(-sy * cp, sp, -cy * cp);
    }

    // I don't know entirely why I need this, but the sphere shader misbehaves otherwise.
    sf::Vector3f getForwardNeg(const CTransform3D& t) {
        float cp = std::cos(t.pitch), sp = std::sin(t.pitch);
        float cy = std::cos(t.yaw),   sy = std::sin(t.yaw);
        return sf::Vector3f(sy * cp, -sp, cy * cp);
    }

    sf::Vector3f getRight(const CTransform3D& t) {
        // Right is the camera's X axis in world space
        // Derived by transforming world X through the inverse camera rotation
        float cy = std::cos(t.yaw), sy = std::sin(t.yaw);
        float cr = std::cos(t.roll), sr = std::sin(t.roll);
        return normalize(sf::Vector3f(cy * cr, sr, -sy * cr));
    }

    sf::Vector3f getUp(const CTransform3D& t) {
        // Up is the cross product of right and forward
        // ensuring an orthonormal basis
        return normalize(cross(getForward(t), getRight(t)));
    }

    sf::Vector3f rotate(const sf::Vector3f& v, float pitch, float yaw, float roll) {
        return worldToCamera(v, pitch, yaw, roll);
    }

    sf::Vector3f rotateInverse(const sf::Vector3f& v, float pitch, float yaw, float roll) {
        return cameraToWorld(v, pitch, yaw, roll);
    }

    sf::Vector3f normalize(const sf::Vector3f& v) {
        float m = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if (m <= 1e-6f) return v;
        return v / m;
    }

    std::array<std::array<float, 3>, 3> getWorldToCamMatrix(float pitch, float yaw, float roll) {
        // Build a matrix that maps world-space vectors into camera-space using the
        // same rotation conventions as `worldToCamera`. We compute the images of
        // the world basis vectors so the result matches the CPU-side math.
        std::array<std::array<float,3>,3> m{};
        sf::Vector3f bx = worldToCamera(sf::Vector3f(1.f, 0.f, 0.f), pitch, yaw, roll);
        sf::Vector3f by = worldToCamera(sf::Vector3f(0.f, 1.f, 0.f), pitch, yaw, roll);
        sf::Vector3f bz = worldToCamera(sf::Vector3f(0.f, 0.f, 1.f), pitch, yaw, roll);

        // toGlslMat3 flattens in column-major order as matrix[0][0], matrix[1][0], matrix[2][0],
        // so store columns as m[0]=bx, m[1]=by, m[2]=bz (each column is an array of 3 rows)
        m[0] = { bx.x, bx.y, bx.z };
        m[1] = { by.x, by.y, by.z };
        m[2] = { bz.x, bz.y, bz.z };
        return m;
    }

    std::array<float, 16> getViewMatrix(const CTransform3D& t) {
        // Get rotation matrix from world vectors -> camera space
        auto rot = getWorldToCamMatrix(t.pitch, t.yaw, t.roll);

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
        float zRange = zNear - zFar; 

        // Standalone Perspective Projection matrix (4x4 column-major)
        // Retains your engine's custom Y-axis inversion (-f)
        return {
            f / c.aspectRatio, 0.f,  0.f,                          0.f,
            0.f,               -f,   0.f,                          0.f,
            0.f,               0.f,  (zFar + zNear) / zRange,     -1.f,   
            0.f,               0.f,  (2.f * zFar * zNear) / zRange, 0.f
        };
    }

    std::array<float, 16> getVPMatrix(const CTransform3D& t, const CCamera& c)
    {
        // Get rotation: world -> camera (columns = transformed basis vectors)
        auto rot = getWorldToCamMatrix(t.pitch, t.yaw, t.roll);

        // rot[col][row] layout from getWorldToCamMatrix:
        // rot[0][0..2] = bx (image of world X)
        // rot[1][0..2] = by
        // rot[2][0..2] = bz

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

        // Projection matrix (standard OpenGL-style perspective, column-major)
        float f     = 1.0f / std::tan(c.fovY * 0.5f);
        float zNear = c.nearPlane;
        float zFar  = c.farPlane;
        float zRange = zNear - zFar;   // negative if zFar > zNear (usual)

        std::array<float, 16> P = {
            f / c.aspectRatio, 0.f,  0.f,                          0.f,
            0.f,               -f,    0.f,                          0.f,
            0.f,               0.f,  (zFar + zNear) / zRange,     -1.f,   // note the -1 for OpenGL convention
            0.f,               0.f,  (2.f * zFar * zNear) / zRange, 0.f
        };

        // P * V
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
