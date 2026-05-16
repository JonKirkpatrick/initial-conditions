#pragma once
           
#include "Animation.hpp"
#include "Assets.h"
#include "UIStructs.h"
#include "Theme.h"
#include <array>

class Entity;


class Component
{
public:
    bool exists = false;
};

class CTransform3D : public Component
{
public:
    sf::Vector3f pos        = { 0.0f, 0.0f,  0.0f };
    sf::Vector3f prevPos    = { 0.0f, 0.0f,  0.0f };
    sf::Vector3f scale      = { 1.0f, 1.0f,  1.0f };
    sf::Vector3f velocity   = { 0.0f, 0.0f,  0.0f };
    float pitch     = 0;
    float yaw       = 0;
    float roll      = 0;
    float speed     = 0;
    bool onGround   = true;

    CTransform3D() = default;
    CTransform3D(const sf::Vector3f & p)
        : pos(p) {}
    CTransform3D(const sf::Vector3f & p, const sf::Vector3f & sp, const sf::Vector3f & sc, float pitch, float yaw, float roll)
        : pos(p), prevPos(p), velocity(sp), scale(sc), pitch(pitch), yaw(yaw), roll(roll) {}
};

class CPlayer : public Component
{
public:
    sf::Vector3f pos        = { 0.0f, 0.0f,  0.0f };
    sf::Vector3f prevPos    = { 0.0f, 0.0f,  0.0f };
    sf::Vector2f facing     = { 0.0f, -1.0f };
    bool onGround           = true;
    bool sprinting          = false;
    bool crouching          = false;
    float moveSpeed         = 0;
    float rotSpeed          = 0;
    float bobAccumulator    = 0;

    CPlayer() = default;
};

class CCamera : public Component
{
public:
    float fovY = 3.14159265f / 4.f;
    float aspectRatio = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.f;
    sf::Vector2u viewportSize = { 800, 600 };

    CCamera() = default;
    CCamera(float fovY, float aspectRatio, float nearPlane, float farPlane, sf::Vector2u viewportSize)
        : fovY(fovY), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane), viewportSize(viewportSize) {}
};

class CInput : public Component
{
public:
    bool up             = false;
    bool down           = false;
    bool left           = false;
    bool right          = false;
    bool forward        = false;
    bool backward       = false;
    bool yawLeft        = false;
    bool yawRight       = false;
    bool pitchUp        = false;
    bool pitchDown      = false;
    bool rollLeft       = false;
    bool rollRight      = false;
    bool strafe         = false;
    bool jump           = false;
    bool sprint         = false;
    bool interact       = false;
    bool crouch         = false;
    float xAxis        = 0.f;
    float yAxis        = 0.f;
    sf::Vector2f mouseDelta     = { 0.f, 0.f };

    CInput() {}
};

class COrb : public Component
{
public:
    sf::Color color = sf::Color::White;
    float radius = 50.0f;
    float bobRate = 2.0f;           // cycles per second
    float bobMagnitude = 8.0f;      // units of vertical movement
    float bobPhase = 0.0f;          // current phase [0, 1)
    float heightAboveGround = 100.0f;

    COrb() = default;
    COrb(const sf::Color& c, float r, float rate = 2.0f, float mag = 8.0f, float heightAbove = 100.0f)
        : color(c), radius(r), bobRate(rate), bobMagnitude(mag), heightAboveGround(heightAbove) {}
};

class CPhysics : public Component
{
public:
    float gravity         = 981.0f;
    float jumpSpeed       = 420.0f;
    
    float groundFriction  = 12.0f;
    float airFriction     = 3.0f;
    
    bool  onGround        = true;
    bool  isCrouching     = false;
    
    float standingHeight  = 1.8f;
    float crouchHeight    = 0.9f;

    CPhysics() = default;
};

static_assert(std::is_default_constructible_v<CTransform3D>);
static_assert(std::is_default_constructible_v<CPlayer>);
static_assert(std::is_default_constructible_v<CCamera>);
static_assert(std::is_default_constructible_v<CInput>);
static_assert(std::is_default_constructible_v<COrb>);
static_assert(std::is_default_constructible_v<CPhysics>);
