#pragma once
#include <glm/glm.hpp>

struct alignas(16) CameraBlock {
    glm::mat4 view;                 // 64 bytes
    glm::mat4 proj;                 // 64 bytes
    glm::mat4 viewProj;             // 64 bytes
    glm::mat4 invViewProj;          // 64 bytes

    glm::vec3 cameraPos;            // 12 bytes
    float     fovY;                 // 4 bytes

    glm::vec3 cameraForward;        // 12 bytes
    float     aspectRatio;          // 4 bytes

    glm::vec3 cameraRight;          // 12 bytes
    float     cameraHeight;         // 4 bytes

    glm::vec3 cameraUp;             // 12 bytes
    float     farPlane;             // 4 bytes

    glm::vec2 viewportSize;         // 8 bytes
    float     nearPlane;            // 4 bytes
    float     _padding;             // 4 bytes
};

struct alignas(16) EnvironmentBlock {
    glm::vec4 sunColor;             // 16 bytes

    glm::vec3 sunDirection;         // 12 bytes
    float     ambientStrength;      // 4 bytes

    glm::vec3 moonDirection;        // 12 bytes
    float     skyExposure;          // 4 bytes

    glm::vec2 windDirection;         // 8 bytes
    float     windSpeed;            // 4 bytes
    float     _padding;             // 4 bytes
};

struct alignas(16) AtmosphereBlock {
    glm::vec4 fogColorDay;          // 16 bytes

    glm::vec4 fogColorNight;        // 16 bytes

    float     fogDensity;           // 4 bytes
    float     fogBaseHeight;        // 4 bytes
    float     fogHeightFalloff;     // 4 bytes
    float     _padding;             // 4 bytes
};