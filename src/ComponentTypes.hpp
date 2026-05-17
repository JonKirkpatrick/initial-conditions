#pragma once

#include "Animation.hpp"
#include "Assets.h"
#include "Theme.h"
#include <array>
#include <SFML/Graphics.hpp>

// Core transform for all entities (POD-style)
struct CTransform3D
{
    sf::Vector3f pos      = { 0.0f, 0.0f, 0.0f };
    sf::Vector3f scale    = { 1.0f, 1.0f, 1.0f };
    sf::Vector3f velocity = { 0.0f, 0.0f, 0.0f };

    float pitch = 0.0f;
    float yaw   = 0.0f;
    float roll  = 0.0f;

    bool onGround = true;

    CTransform3D() = default;
    CTransform3D(const sf::Vector3f & p) : pos(p) {}
    CTransform3D(const sf::Vector3f & p, const sf::Vector3f & vel, const sf::Vector3f & sc,
                 float pPitch, float pYaw, float pRoll)
        : pos(p), velocity(vel), scale(sc), pitch(pPitch), yaw(pYaw), roll(pRoll) {}
};

// Physics + Locomotion state (POD)
struct CPhysics
{
    float gravity         = 981.0f;
    float jumpSpeed       = 420.0f;
    
    float groundFriction  = 12.0f;
    float airFriction     = 3.0f;
    
    bool  onGround        = true;
    bool  isCrouching     = false;
    bool  isSprinting     = false;
    
    float standingHeight  = 1.8f;
    float crouchHeight    = 0.9f;

    CPhysics() = default;
};

// Bobbing behavior (POD)
struct CBob
{
    float accumulator = 0.0f;   // phase [0, 1)
    float rate        = 1.0f;   // base cycles per second
    float magnitude   = 6.0f;   // vertical bob
    float lateralMag  = 5.0f;   // side-to-side (mainly for player)

    CBob() = default;
    CBob(float r, float mag, float lat = 5.0f)
        : rate(r), magnitude(mag), lateralMag(lat) {}
};

// Player identity / player-only data (POD)
struct CPlayer
{
    CPlayer() = default;
};

// Camera (POD)
struct CCamera
{
    float fovY = 3.14159265f / 4.f;
    float aspectRatio = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.f;
    sf::Vector2u viewportSize = { 800, 600 };

    CCamera() = default;
    CCamera(float fovY, float aspectRatio, float nearPlane, float farPlane, sf::Vector2u viewportSize)
        : fovY(fovY), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane), viewportSize(viewportSize) {}
};

// Input (only attached to player) (POD)
struct CInput
{
    bool forward  = false;
    bool backward = false;
    bool left     = false;
    bool right    = false;
    bool strafe   = false;
    bool jump     = false;
    bool sprint   = false;
    bool interact = false;
    bool crouch   = false;

    float xAxis = 0.f;
    float yAxis = 0.f;
    sf::Vector2f mouseDelta = { 0.f, 0.f };

    CInput() = default;
};

// Orb (POD)
struct COrb
{
    sf::Color color = sf::Color::White;
    float radius = 50.0f;
    float heightAboveGround = 100.0f;

    COrb() = default;
    COrb(const sf::Color& c, float r, float heightAbove = 100.0f)
        : color(c), radius(r), heightAboveGround(heightAbove) {}
};

static_assert(std::is_default_constructible_v<CTransform3D>);
static_assert(std::is_default_constructible_v<CPhysics>);
static_assert(std::is_default_constructible_v<CBob>);
static_assert(std::is_default_constructible_v<CPlayer>);
static_assert(std::is_default_constructible_v<CCamera>);
static_assert(std::is_default_constructible_v<CInput>);
static_assert(std::is_default_constructible_v<COrb>);
