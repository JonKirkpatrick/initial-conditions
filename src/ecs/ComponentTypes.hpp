#pragma once

/**
 * @file Components.h
 * @brief Component definitions for the Entity Component System (ECS).
 * 
 * Contains all pure data components attached to entities for physics, 3D transformations,
 * camera projection, player input, locomotion/gait, visual appearance, and state tracking.
 */

#include "core/Assets.h"
#include "ui/Theme.h"
#include <array>
#include <type_traits>
#include <SFML/Graphics.hpp>
#include <glm/gtc/quaternion.hpp>

/// @name Spatial & Transform Components
/// @{

/**
 * @brief 3D Spatial Transform component tracking position, rotation quaternion, and scale.
 * 
 * Maintains orientation using quaternions and caches directional vectors (\f$\vec{forward}\f$, 
 * \f$\vec{right}\f$, \f$\vec{up}\f$). Uses a dirty flag pattern so downstream rendering/physics 
 * systems only recalculate transform matrices when spatial state changes.
 */
struct CTransform3D
{
    sf::Vector3f pos      = { 0.0f, 0.0f, 0.0f }; ///< World position vector \f$(x, y, z)\f$ in meters.
    sf::Vector3f scale    = { 1.0f, 1.0f, 1.0f }; ///< World scale multiplier along local axes.
    sf::Vector3f velocity = { 0.0f, 0.0f, 0.0f }; ///< Linear velocity vector in meters per second.

private:
    glm::quat m_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); ///< Unit rotation quaternion.

    sf::Vector3f m_forward = {  0.0f,  0.0f, -1.0f }; ///< Normalized forward directional vector.
    sf::Vector3f m_right   = {  1.0f,  0.0f,  0.0f }; ///< Normalized right directional vector.
    sf::Vector3f m_up      = {  0.0f,  1.0f,  0.0f }; ///< Normalized up directional vector.

    bool m_isDirty = true; ///< Dirty flag indicating cached orientation vectors require re-computation.

public:
    bool onGround = true; ///< Flag indicating whether entity transform is grounded.

    /** @brief Default constructor initializing identity transform. */
    CTransform3D() = default;

    /**
     * @brief Constructs transform with specified initial position.
     * @param p Initial 3D world position vector.
     */
    CTransform3D(const sf::Vector3f & p) : pos(p) {}

    /**
     * @brief Gets the current orientation quaternion.
     * @return Const reference to unit quaternion.
     */
    const glm::quat& orientation() const noexcept { return m_orientation; }

    /**
     * @brief Gets cached forward vector.
     * @return Const reference to unit forward vector.
     */
    const sf::Vector3f& forward() const noexcept  { return m_forward; }

    /**
     * @brief Gets cached right vector.
     * @return Const reference to unit right vector.
     */
    const sf::Vector3f& right() const noexcept    { return m_right; }

    /**
     * @brief Gets cached up vector.
     * @return Const reference to unit up vector.
     */
    const sf::Vector3f& up() const noexcept       { return m_up; }

    /**
     * @brief Checks if orientation vectors need recalculation.
     * @return `true` if transform changed since last sync, `false` otherwise.
     */
    bool isDirty() const noexcept                 { return m_isDirty; }

    /** @brief Clears the dirty state flag following vector sync. */
    void clean() noexcept { m_isDirty = false; }

    /**
     * @brief Directly sets cached directional basis vectors.
     * @param f Normalized forward vector.
     * @param r Normalized right vector.
     * @param u Normalized up vector.
     */
    void setCachedVectors(const sf::Vector3f& f, const sf::Vector3f& r, const sf::Vector3f& u) noexcept {
        m_forward = f; m_right = r; m_up = u;
    }

    /**
     * @brief Sets absolute rotation from Euler angles in degrees.
     * @param pitchDeg Rotation around local X-axis (degrees).
     * @param yawDeg Rotation around local Y-axis (degrees).
     * @param rollDeg Rotation around local Z-axis (degrees).
     */
    void setRotation(float pitchDeg, float yawDeg, float rollDeg) noexcept {
        glm::quat p = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(1.f, 0.f, 0.f));
        glm::quat y = glm::angleAxis(glm::radians(yawDeg),   glm::vec3(0.f, 1.f, 0.f));
        glm::quat r = glm::angleAxis(glm::radians(rollDeg),  glm::vec3(0.f, 0.f, 1.f));
        
        m_orientation = y * p * r;
        m_isDirty = true;
    }

    /**
     * @brief Sets absolute orientation directly from quaternion.
     * @param q New orientation quaternion.
     */
    void setOrientation(const glm::quat& q) noexcept {
        m_orientation = q;
        m_isDirty = true;
    }

    /**
     * @brief Applies incremental local Euler rotation in degrees.
     * @param pitchDelta Incremental pitch change (degrees).
     * @param yawDelta Incremental yaw change (degrees).
     * @param rollDelta Incremental roll change (degrees).
     */
    void addLocalRotation(float pitchDelta, float yawDelta, float rollDelta) noexcept {
        if (pitchDelta != 0.f) m_orientation = m_orientation * glm::angleAxis(glm::radians(pitchDelta), glm::vec3(1.f, 0.f, 0.f));
        if (yawDelta != 0.f)   m_orientation = m_orientation * glm::angleAxis(glm::radians(yawDelta),   glm::vec3(0.f, 1.f, 0.f));
        if (rollDelta != 0.f)  m_orientation = m_orientation * glm::angleAxis(glm::radians(rollDelta),  glm::vec3(0.f, 0.f, 1.f));
        
        m_orientation = glm::normalize(m_orientation);
        m_isDirty = true;
    }
};

/// @}

/// @name Movement & Locomotion Components
/// @{

/**
 * @brief Dynamic physical constants, constraints, and surface friction attributes.
 */
struct CPhysics
{
    float gravity         = 9.81f;  ///< Downward acceleration due to gravity (\f$\text{m/s}^2\f$).
    float jumpSpeed       = 4.20f;  ///< Initial upward impulse velocity when jumping (\f$\text{m/s}\f$).
    
    float groundFriction  = 12.0f;  ///< Ground movement deceleration coefficient.
    float airFriction     = 3.0f;   ///< Air resistance drag coefficient when airborne.
    
    bool  onGround        = true;   ///< `true` if character entity is resting on walkable terrain.
    bool  isCrouching     = false;  ///< `true` if entity is in crouch stance.
    bool  isSprinting     = false;  ///< `true` if entity is currently sprinting.
    
    float standingHeight  = 1.8f;   ///< Collider/camera height when fully standing (meters).
    float crouchHeight    = 0.9f;   ///< Collider/camera height when crouching (meters).

    CPhysics() = default;
};

/**
 * @brief Legacy bobbing component.
 * @deprecated Scheduled for removal/refactoring into `CGaitCycle` and `CKinematicBob`.
 */
struct CBob
{
    float accumulator = 0.0f;   ///< Cycle phase phase in normalized range \f$[0.0, 1.0)\f$.
    float rate        = 1.0f;   ///< Frequency in cycles per second.
    float magnitude   = 6.0f;   ///< Vertical bob displacement height.
    float lateralMag  = 5.0f;   ///< Horizontal/side-to-side sway displacement width.

    CBob() = default;

    /**
     * @param r Base cycles per second.
     * @param mag Vertical bob amplitude.
     * @param lat Side-to-side sway amplitude.
     */
    CBob(float r, float mag, float lat = 5.0f)
        : rate(r), magnitude(mag), lateralMag(lat) {}
};

/**
 * @brief Presentation and gait-event state tracker for player footstep audio and camera bobbing.
 * 
 * Drives visual view movement and footfall sound triggers without directly altering physical position.
 * Updated by the gait system after velocity resolution.
 */
struct CGaitCycle
{
    float accumulator      = 0.0f;   ///< Current step phase normalized range \f$[0.0, 1.0)\f$.
    float lastPhase        = 0.0f;   ///< Previous frame phase for detecting footfall phase transitions.
    float strideRate       = 0.020f; ///< Base step cycles per traveled distance unit.
    float bobMagnitude     = 0.1f;   ///< Vertical camera oscillation amplitude.
    float lateralMagnitude = 0.2f;   ///< Side-to-side camera sway oscillation amplitude.

    CGaitCycle() = default;

    /**
     * @param phase Current cycle phase \f$[0, 1)\f$.
     * @param prevPhase Previous frame cycle phase.
     * @param rate Stride rate multiplier.
     * @param vertMag Vertical displacement amplitude.
     * @param latMag Lateral displacement amplitude.
     */
    CGaitCycle(float phase, float prevPhase, float rate, float vertMag, float latMag)
        : accumulator(phase), lastPhase(prevPhase), strideRate(rate), bobMagnitude(vertMag), lateralMagnitude(latMag) {}
};

/**
 * @brief Kinematic bobbing component for non-player entities (e.g., hover orbs, collectibles).
 * 
 * Unlike `CGaitCycle`, this phase accumulator is directly consumed by spatial movement systems 
 * to alter physical entity position.
 */
struct CKinematicBob
{
    float accumulator = 0.0f;   ///< Motion phase normalized range \f$[0.0, 1.0)\f$.
    float rate        = 1.0f;   ///< Oscillation rate in cycles per second.
    float amplitude   = 6.0f;   ///< Peak vertical displacement distance.

    CKinematicBob() = default;

    /**
     * @param r Oscillation rate in Hz.
     * @param mag Peak displacement magnitude.
     */
    CKinematicBob(float r, float mag) : rate(r), amplitude(mag) {}
};

/// @}

/// @name Camera & Input Components
/// @{

/**
 * @brief Tag component designating an entity as the active local player.
 */
struct CPlayer
{
    CPlayer() = default;
};

/**
 * @brief 3D Perspective camera projection parameters.
 */
struct CCamera
{
    float fovY = 3.14159265f / 4.f;    ///< Vertical field of view in radians (default: \f$\frac{\pi}{4}\f$ rad / 45°).
    float aspectRatio = 1.0f;           ///< Screen aspect ratio (\f$\text{width} / \text{height}\f$).
    float nearPlane = 0.1f;             ///< Distance to near clipping plane in meters.
    float farPlane = 1000.f;            ///< Distance to far clipping plane in meters.
    sf::Vector2u viewportSize = { 800, 600 }; ///< Dimensions of rendered viewport in pixels.

    CCamera() = default;

    /**
     * @param fovY Vertical FOV in radians.
     * @param aspectRatio Width divided by height.
     * @param nearPlane Distance to near clip plane.
     * @param farPlane Distance to far clip plane.
     * @param viewportSize Pixel dimensions of output window.
     */
    CCamera(float fovY, float aspectRatio, float nearPlane, float farPlane, sf::Vector2u viewportSize)
        : fovY(fovY), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane), viewportSize(viewportSize) {}
};

/**
 * @brief Per-frame input state accumulator captured from hardware events.
 */
struct CInput
{
    bool forward  = false; ///< Directional move forward pressed state.
    bool backward = false; ///< Directional move backward pressed state.
    bool left     = false; ///< Directional move left pressed state.
    bool right    = false; ///< Directional move right pressed state.
    bool strafe   = false; ///< Strafe modifier key pressed state.
    bool jump     = false; ///< Jump key pressed state.
    bool sprint   = false; ///< Sprint modifier key pressed state.
    bool interact = false; ///< Interaction key pressed state.
    bool crouch   = false; ///< Crouch stance key pressed state.

    float xAxis = 0.f; ///< Normalized horizontal input axis value \f$[-1.0, 1.0]\f$.
    float yAxis = 0.f; ///< Normalized vertical input axis value \f$[-1.0, 1.0]\f$.
    sf::Vector2f mouseDelta = { 0.f, 0.f }; ///< Frame mouse movement delta \f$(\Delta x, \Delta y)\f$.

    CInput() = default;
};

/// @}

/// @name Visual & Entity Customization Components
/// @{

/**
 * @brief Properties and orientation basis for interactive orb entities.
 */
struct COrb
{
    float radius        = 50.0f; ///< Collision and bounding radius of the orb mesh.

    sf::Vector3f forward = { 0.0f, 0.0f, 1.0f }; ///< Explicit forward orientation vector.
    sf::Vector3f right   = { 1.0f, 0.0f, 0.0f }; ///< Explicit right orientation vector.
    sf::Vector3f up      = { 0.0f, 1.0f, 0.0f }; ///< Explicit up orientation vector.

    int          speciesIdx     = 6;     ///< Visual texture/species palette index.

    COrb() = default;

    /**
     * @param r Mesh bounding radius.
     */
    COrb(float r) : radius(r) {}
};

/**
 * @brief Procedural eye animation and gaze tracking parameters.
 */
struct CEyes
{
    sf::Vector2f gazeDirection  = { 0.0f, 0.0f }; ///< Normalized local gaze tracking offset vector \f$(x, y)\f$.
    float        pupilDilation  = 0.5f;           ///< Pupil dilation factor in normalized range \f$[0.0, 1.0]\f$.
    float        eyelidClosure  = 0.0f;           ///< Eyelid closure ratio in normalized range \f$[0.0, 1.0]\f$ (0 = fully open, 1 = closed).

    CEyes() = default;
};

/// @}

// Type safety checks ensuring all components satisfy default constructibility guarantees for ECS registry storage.
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