#pragma once

#include "core/Assets.h"
#include "ui/Theme.h"
#include <array>
#include <SFML/Graphics.hpp>
#include <glm/gtc/quaternion.hpp>

struct CTransform3D
{
    sf::Vector3f pos      = { 0.0f, 0.0f, 0.0f };
    sf::Vector3f scale    = { 1.0f, 1.0f, 1.0f };
    sf::Vector3f velocity = { 0.0f, 0.0f, 0.0f };

private:
    glm::quat m_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    sf::Vector3f m_forward = {  0.0f,  0.0f, -1.0f };
    sf::Vector3f m_right   = {  1.0f,  0.0f,  0.0f };
    sf::Vector3f m_up      = {  0.0f,  1.0f,  0.0f };

    bool m_isDirty = true;

public:
    bool onGround = true;

    CTransform3D() = default;
    CTransform3D(const sf::Vector3f & p) : pos(p) {}

    const glm::quat& orientation() const noexcept { return m_orientation; }
    const sf::Vector3f& forward() const noexcept  { return m_forward; }
    const sf::Vector3f& right() const noexcept    { return m_right; }
    const sf::Vector3f& up() const noexcept       { return m_up; }
    bool isDirty() const noexcept                 { return m_isDirty; }

    void clean() noexcept { m_isDirty = false; }
    void setCachedVectors(const sf::Vector3f& f, const sf::Vector3f& r, const sf::Vector3f& u) noexcept {
        m_forward = f; m_right = r; m_up = u;
    }

    void setRotation(float pitchDeg, float yawDeg, float rollDeg) noexcept {
        glm::quat p = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(1.f, 0.f, 0.f));
        glm::quat y = glm::angleAxis(glm::radians(yawDeg),   glm::vec3(0.f, 1.f, 0.f));
        glm::quat r = glm::angleAxis(glm::radians(rollDeg),  glm::vec3(0.f, 0.f, 1.f));
        
        m_orientation = y * p * r;
        m_isDirty = true;
    }

    void setOrientation(const glm::quat& q) noexcept {
        m_orientation = q;
        m_isDirty = true;
    }

    void addLocalRotation(float pitchDelta, float yawDelta, float rollDelta) noexcept {
        if (pitchDelta != 0.f) m_orientation = m_orientation * glm::angleAxis(glm::radians(pitchDelta), glm::vec3(1.f, 0.f, 0.f));
        if (yawDelta != 0.f)   m_orientation = m_orientation * glm::angleAxis(glm::radians(yawDelta),   glm::vec3(0.f, 1.f, 0.f));
        if (rollDelta != 0.f)  m_orientation = m_orientation * glm::angleAxis(glm::radians(rollDelta),  glm::vec3(0.f, 0.f, 1.f));
        
        m_orientation = glm::normalize(m_orientation);
        m_isDirty = true;
    }
};

struct CPhysics
{
    float gravity         = 9.81f;
    float jumpSpeed       = 4.20f;
    
    float groundFriction  = 12.0f;
    float airFriction     = 3.0f;
    
    bool  onGround        = true;
    bool  isCrouching     = false;
    bool  isSprinting     = false;
    
    float standingHeight  = 1.8f;
    float crouchHeight    = 0.9f;

    CPhysics() = default;
};

// This is on the chopping block to be replaced with a pair of components so they can fulfill their intended roles.
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

// Player-only. Pure presentation/gait-event state — never touches position.
// Read by: camera (bob offset), footstep-audio system.
// Written by: gait system only, after sMovement has settled velocity/onGround.
struct CGaitCycle
{
    float accumulator      = 0.0f;   // phase [0, 1)
    float lastPhase        = 0.0f;   // previous frame's phase, for edge-detecting footfalls
    float strideRate       = 0.020f; // base cycles per "unit," matches your old baseRate constant
    float bobMagnitude     = 0.1f;   // vertical bob amplitude
    float lateralMagnitude = 0.2f;  // side-to-side sway amplitude

    CGaitCycle() = default;
    CGaitCycle(float phase, float prevPhase, float rate, float vertMag, float latMag)
        : accumulator(phase), lastPhase(prevPhase), strideRate(rate), bobMagnitude(vertMag), lateralMagnitude(latMag) {}
};

// Non-player kinematic bobbers (orbs, etc). Accumulator IS the motion —
// consumed directly as a literal position term in resolveEntityPosition/updateOrbBobbing.
struct CKinematicBob
{
    float accumulator = 0.0f;   // phase [0, 1)
    float rate        = 1.0f;   // cycles per second
    float amplitude   = 6.0f;   // vertical displacement

    CKinematicBob() = default;
    CKinematicBob(float r, float mag) : rate(r), amplitude(mag) {}
};

struct CPlayer
{
    CPlayer() = default;
};

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

struct COrb
{
    float radius        = 50.0f;

    // Orientation basis — previously implicit, now explicit
    sf::Vector3f forward = { 0.0f, 0.0f, 1.0f };
    sf::Vector3f right   = { 1.0f, 0.0f, 0.0f };
    sf::Vector3f up      = { 0.0f, 1.0f, 0.0f };

    // Visual properties
    int          speciesIdx     = 6;

    COrb() = default;
    COrb(float r) : radius(r) {}
};

struct CEyes
{
    sf::Vector2f gazeDirection  = { 0.0f, 0.0f };
    float        pupilDilation  = 0.5f;
    float        eyelidClosure  = 0.0f;

    CEyes() = default;
};

static_assert(std::is_default_constructible_v<CTransform3D>);
static_assert(std::is_default_constructible_v<CPhysics>);
static_assert(std::is_default_constructible_v<CBob>);
static_assert(std::is_default_constructible_v<CGaitCycle>);
static_assert(std::is_default_constructible_v<CKinematicBob>);
static_assert(std::is_default_constructible_v<CPlayer>);
static_assert(std::is_default_constructible_v<CCamera>);
static_assert(std::is_default_constructible_v<CInput>);
static_assert(std::is_default_constructible_v<COrb>);
static_assert(std::is_default_constructible_v<CEyes>);
