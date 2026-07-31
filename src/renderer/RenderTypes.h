/**
 * @file RenderTypes.h
 * @brief Uniform Buffer Object (UBO) data structures for GPU rendering pipelines.
 * 
 * Contains 16-byte aligned structures formatted for GLSL `std140` uniform blocks, 
 * housing view/projection camera matrices, atmospheric fog parameters, and dynamic sky/sun 
 * environmental lighting metrics.
 */

#pragma once
#include <glm/glm.hpp>

/**
 * @brief GPU camera parameters block aligned for std140 UBO layout.
 * 
 * Total size: 288 bytes (aligned to 16-byte boundaries).
 */
struct alignas(16) CameraBlock {
    glm::mat4 view;                 ///< 4x4 View matrix (64 bytes).
    glm::mat4 proj;                 ///< 4x4 Projection matrix (64 bytes).
    glm::mat4 viewProj;             ///< 4x4 View-Projection matrix (64 bytes).
    glm::mat4 invViewProj;          ///< 4x4 Inverse View-Projection matrix (64 bytes).

    glm::vec3 cameraPos;            ///< Camera world position coordinates (12 bytes).
    float     fovY;                 ///< Vertical field of view in radians/degrees (4 bytes).

    glm::vec3 cameraForward;        ///< Normalized camera forward direction vector (12 bytes).
    float     aspectRatio;          ///< Viewport aspect ratio [Width / Height] (4 bytes).

    glm::vec3 cameraRight;          ///< Normalized camera right direction vector (12 bytes).
    float     cameraHeight;         ///< Camera height above ground level (4 bytes).

    glm::vec3 cameraUp;             ///< Normalized camera up direction vector (12 bytes).
    float     farPlane;             ///< Far clip plane distance (4 bytes).

    glm::vec2 viewportSize;         ///< Screen dimensions in pixels [width, height] (8 bytes).
    float     nearPlane;            ///< Near clip plane distance (4 bytes).
    float     _padding;             ///< Explicit 4-byte padding field for 16-byte alignment.
};

/**
 * @brief GPU environmental lighting parameters block aligned for std140 UBO layout.
 * 
 * Total size: 64 bytes (aligned to 16-byte boundaries).
 */
struct alignas(16) EnvironmentBlock {
    glm::vec4 sunColor;             ///< Direct sunlight RGBA color tint and intensity scale (16 bytes).

    glm::vec3 sunDirection;         ///< Normalized unit vector pointing toward the sun (12 bytes).
    float     ambientStrength;      ///< Ambient sky lighting factor (4 bytes).

    glm::vec3 moonDirection;        ///< Normalized unit vector pointing toward the moon (12 bytes).
    float     skyExposure;          ///< Skybox tone-mapping exposure coefficient (4 bytes).

    glm::vec2 windDirection;        ///< Normalized 2D world space wind vector (8 bytes).
    float     windSpeed;            ///< Wind movement magnitude scalar (4 bytes).
    float     _padding;             ///< Explicit 4-byte padding field for 16-byte alignment.
};

/**
 * @brief GPU atmospheric fog parameters block aligned for std140 UBO layout.
 * 
 * Total size: 48 bytes (aligned to 16-byte boundaries).
 */
struct alignas(16) AtmosphereBlock {
    glm::vec4 fogColorDay;          ///< Daytime horizon fog RGBA color tint (16 bytes).

    glm::vec4 fogColorNight;        ///< Nighttime horizon fog RGBA color tint (16 bytes).

    float     fogDensity;           ///< Global atmospheric fog density scalar (4 bytes).
    float     fogBaseHeight;        ///< Reference altitude origin for height-based fog (4 bytes).
    float     fogHeightFalloff;     ///< Vertical height density falloff rate parameter (4 bytes).
    float     _padding;             ///< Explicit 4-byte padding field for 16-byte alignment.
};