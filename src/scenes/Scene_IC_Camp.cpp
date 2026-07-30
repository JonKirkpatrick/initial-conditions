#include <GL/glew.h>
#include "scenes/Scene_IC_Camp.h"
#include "core/WorldCoordinates.hpp"
#include "core/GameEngine.h"
#include "core/Assets.h"
#include "renderer/OrbSSBO.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics/CoordinateType.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui-SFML.h"
#include "renderer/Camera.h"
#include "environment/Astro.hpp"
#include "environment/TerrainStreamer.h"
#include "renderer/ShaderLocations.h"
#include <random>
#include <array>
#include <filesystem>
#include <fstream>

// =========================================================================
// File-Local Static Helper Utilities
// =========================================================================
static glm::vec3 toGLMVec3(const sf::Vector3f& v) 
{
    return glm::vec3(v.x, v.y, v.z);
}

static glm::vec4 toGLMVec4(const sf::Glsl::Vec4& v) 
{
    return glm::vec4(v.x, v.y, v.z, v.w);
}

static sf::Vector3f forwardFromTransform(const CTransform3D& transform)
{
    return Camera::cameraToWorld(sf::Vector3f(0.f, 0.f, -1.f), transform);
}

static float length(const sf::Vector3f& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float dot(const sf::Vector3f& a, const sf::Vector3f& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static sf::Vector3f cross(const sf::Vector3f& a, const sf::Vector3f& b)
{
    return sf::Vector3f(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

// =========================================================================
// Namespace Aliases
// =========================================================================

namespace WCS = WorldCoordinates::Square;
namespace WCH = WorldCoordinates::Hex;

// =========================================================================
// Public Interface
// =========================================================================

Scene_IC_Camp::Scene_IC_Camp(GameEngine& game, const std::string& levelPath)
    : Scene(game)
    , m_levelPath(levelPath)
{
    initSceneConfiguration();
    initGraphicsPipelines();
    initLevelState();
}

Scene_IC_Camp::~Scene_IC_Camp() 
{
    cleanUpGraphicsResources();
}

void Scene_IC_Camp::update() 
{
    // 1. Core Framework Timing Updates
    float currentTime = m_game.getElapsedClock().getElapsedTime().asSeconds();
    float dt = 1.0f / 60.0f;
    if (m_lastFrameTime > 0.0f) {
        dt = std::clamp(currentTime - m_lastFrameTime, 0.0001f, 0.5f);
    }
    m_lastFrameTime = currentTime;

    // 2. Cursor State Bookkeeping
    if (m_cursorMode) {
        m_cachedMousePos   = sf::Mouse::getPosition(m_game.window());
        m_leftMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    }

    // 3. Delegate Astronomical Clock Progress Outward
    constexpr float kRealSecondsPerGameHour = 3600.0f;
    Astro::advanceCalendar(dt, kRealSecondsPerGameHour, m_gameTimeOfDay, 
                           m_gameDayOfMonth, m_gameMonth, m_gameYear);

    // 4. Systems Phase Updates
    sMovement(dt);
    sGaitAndFootsteps(dt);
    
    sf::Vector3f playerPos = m_entityManager.getTransform(m_player).pos;
    m_terrainStreamer->update({playerPos.x, playerPos.z});
    
    m_entityManager.sUpdateTransformVectors();
    updateCamera(dt);
    updateHUDData();
    updateAstronomySystem();
    updateOrbShaderStorage();
    
    m_hud->update(m_game.window(), m_hudData);
    if (m_showGUI) {
        sGUI();
    }

    // 5. Diagnostics Meter Trackers
    m_fpsFrameCount++;
    if (m_fpsClock.getElapsedTime().asSeconds() >= 0.5f) {
        m_fps = float(m_fpsFrameCount) / m_fpsClock.restart().asSeconds();
        m_fpsFrameCount = 0;
    }

    // 6. Terminal Entity Pipeline Update (Synchronize creation/deletions at the frame boundary)
    m_entityManager.update();
}

void Scene_IC_Camp::sDoAction(const Action& action) 
{
    auto& input = m_entityManager.getInput(m_player);

    if (action.type() == "ANALOG") {
        if (action.name() == InputAction::CameraYawRight) {
            input.xAxis = action.value();
        } else if (action.name() == InputAction::MoveUp) {
            input.yAxis = action.value();
        }
    }

    if (action.type() == "LOOK") {
        input.mouseDelta = action.delta();
    }

    if (action.type() == "START") {
        if (action.name() == InputAction::Quit) {
            if (m_game.isMouseCaptured()) {
                m_game.setMouseCaptured(false);
            }
            m_game.changeScene("MENU", nullptr, false, false);
        }
        else if (action.name() == InputAction::ToggleCursor) {
            m_game.setMouseCaptured(!m_game.isMouseCaptured());
            m_cursorMode = !m_game.isMouseCaptured();
            sf::Vector2u size = m_game.window().getSize();
            m_entityManager.getInput(m_player).mouseDelta = {0.f, 0.f};
            sf::Mouse::setPosition(
                sf::Vector2i(size.x / 2, size.y / 2), 
                m_game.window()
            );
        }
        else if (m_cursorMode) { return; }
        else if (action.name() == InputAction::MoveForward)  { input.forward  = true; }
        else if (action.name() == InputAction::MoveBackward) { input.backward = true; }
        else if (action.name() == InputAction::MoveLeft)     { input.left     = true; }
        else if (action.name() == InputAction::MoveRight)    { input.right    = true; }
        else if (action.name() == InputAction::Strafe)       { input.strafe   = true; }
        else if (action.name() == InputAction::Jump)         { input.jump     = true; }
        else if (action.name() == InputAction::Crouch)       { input.crouch   = true; }
        else if (action.name() == InputAction::Sprint)       { input.sprint   = true; }
        else if (action.name() == InputAction::Interact)     { input.interact = true; }
        else if (action.name() == InputAction::ShowGui)      { m_showGUI = !m_showGUI; }
        else if (action.name() == InputAction::ToggleHeadlight) {
            if (m_headlightState == HeadlightState::Off) {
                m_headlightState = HeadlightState::On;
            } else if (m_headlightState == HeadlightState::On) {
                m_headlightState = HeadlightState::Auto;
            } else {
                m_headlightState = HeadlightState::Off;
            }
        }
    }

    else if (action.type() == "END") {
        if      (action.name() == InputAction::MoveForward)  { input.forward  = false; }
        else if (action.name() == InputAction::MoveBackward) { input.backward = false; }
        else if (action.name() == InputAction::MoveLeft)     { input.left     = false; }
        else if (action.name() == InputAction::MoveRight)    { input.right    = false; }
        else if (action.name() == InputAction::Strafe)       { input.strafe   = false; }
        else if (action.name() == InputAction::Jump)         { input.jump     = false; }
        else if (action.name() == InputAction::Crouch)       { input.crouch   = false; }
        else if (action.name() == InputAction::Sprint)       { input.sprint   = false; }
        else if (action.name() == InputAction::Interact)     { input.interact = false; }
    }
}

void Scene_IC_Camp::onEnter() 
{
    m_entityManager.getInput(m_player).mouseDelta = {0.f, 0.f};
    sf::Vector2u size = m_game.window().getSize();
    sf::Mouse::setPosition(
        sf::Vector2i(size.x / 2, size.y / 2), 
        m_game.window()
    );
    m_game.setMouseCaptured(true);

    m_game.window().setActive(true); 
    
    m_cameraConfig.VIEWPORT_WIDTH  = size.x;
    m_cameraConfig.VIEWPORT_HEIGHT = size.y;
    
    m_gBufferWidth  = size.x;
    m_gBufferHeight = size.y;
    
    auto& cameraData = m_entityManager.getCamera(m_camera);
    cameraData.aspectRatio = float(size.x) / float(size.y);
    cameraData.viewportSize = size;
}

void Scene_IC_Camp::onExit() 
{
}

void Scene_IC_Camp::onEnd() 
{
}

HUD* Scene_IC_Camp::getHUD() const
{
    return m_hud.get();
}

Topography::TerrainContext Scene_IC_Camp::getTerrainContext() const 
{
    return Topography::TerrainContext {
        m_terrainStreamer.get(),
        m_topdownWorldMin,
        m_topdownWorldSize,
    };
}

void Scene_IC_Camp::initSceneConfiguration() 
{
    m_topdownMaxHeight = 1000.f;
    m_topdownWorldMin   = { 0.f, 0.f };
    m_topdownWorldSize  = { Topography::BASE_SIZE, Topography::BASE_SIZE };
    m_gridColour        = Theme::color("tarmac");

    sf::Vector2u windowSize        = m_game.window().getSize();
    m_cameraConfig.VIEWPORT_WIDTH  = windowSize.x;
    m_cameraConfig.VIEWPORT_HEIGHT = windowSize.y;
}

void Scene_IC_Camp::initGraphicsPipelines() 
{
    initializeGBuffer(m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT);
    initializeSkyCubemap();
    generateOceanMesh(m_oceanSize, m_oceanResolution, m_oceanVAO, m_oceanVBO, m_oceanEBO, m_oceanIndexCount);
    initializeShadowMap(m_shadowMapSize);
    initSSAONoiseTexture();
    initSSAOKernel();
    initSSAOFramebuffers();
    initUBOs();
}

void Scene_IC_Camp::initLevelState() 
{
    m_moonTexture     = Assets::Instance().getTexture("Moon");
    m_skyTexture     = sf::RenderTexture({m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT});
    m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});

    loadLevel(m_levelPath);
    spawnPlayer();
    spawnCamera();
    spawnDebugOrbs(64000);
    m_entityManager.update();
    initializeOrbShaderStorage();

    buildTerrainGrid();
    buildHud();
    
    updateHUDData();
    updateAstronomySystem();

    m_game.setMouseCaptured(true);
    m_cursorMode = false;
}

void Scene_IC_Camp::cleanUpGraphicsResources() 
{
    destroyGBuffer();
    destroyShadowMap();
    m_ssaoPipeline.destroy();

    if (m_skyCubemapHandle != 0) {
        glDeleteTextures(1, &m_skyCubemapHandle);
        m_skyCubemapHandle = 0;
    }

    if (m_cameraUBO != 0) {
        glDeleteBuffers(1, &m_cameraUBO);
        m_cameraUBO = 0;
    }
    if (m_envUBO != 0) {
        glDeleteBuffers(1, &m_envUBO);
        m_envUBO = 0;
    }
    if (m_atmoUBO != 0) {
        glDeleteBuffers(1, &m_atmoUBO);
        m_atmoUBO = 0;
    }

    if (m_gridVAO != 0) {
        glDeleteVertexArrays(1, &m_gridVAO);
        m_gridVAO = 0;
    }
    if (m_gridVBO != 0) {
        glDeleteBuffers(1, &m_gridVBO);
        m_gridVBO = 0;
    }
    if (m_gridEBO != 0) {
        glDeleteBuffers(1, &m_gridEBO);
        m_gridEBO = 0;
    }

    if (m_cubeVAO != 0) {
        glDeleteVertexArrays(1, &m_cubeVAO);
        m_cubeVAO = 0;
    }
    if (m_cubeVBO != 0) {
        glDeleteBuffers(1, &m_cubeVBO);
        m_cubeVBO = 0;
    }
    if (m_cubeEBO != 0) {
        glDeleteBuffers(1, &m_cubeEBO);
        m_cubeEBO = 0;
    }

    if (m_blitVAO != 0) {
        glDeleteVertexArrays(1, &m_blitVAO);
        m_blitVAO = 0;
    }
    if (m_blitVBO != 0) {
        glDeleteBuffers(1, &m_blitVBO);
        m_blitVBO = 0;
    }

    if (m_lightingVAO != 0) {
        glDeleteVertexArrays(1, &m_lightingVAO);
        m_lightingVAO = 0;
    }
    if (m_lightingVBO != 0) {
        glDeleteBuffers(1, &m_lightingVBO);
        m_lightingVBO = 0;
    }

    if (m_oceanVAO != 0) {
        glDeleteVertexArrays(1, &m_oceanVAO);
        m_oceanVAO = 0;
    }
    if (m_oceanVBO != 0) {
        glDeleteBuffers(1, &m_oceanVBO);
        m_oceanVBO = 0;
    }
    if (m_oceanEBO != 0) {
        glDeleteBuffers(1, &m_oceanEBO);
        m_oceanEBO = 0;
    }
}

void Scene_IC_Camp::sRender() 
{
    auto& window = m_game.window();
    auto& transform = m_entityManager.getTransform(m_camera);
    auto& camData = m_entityManager.getCamera(m_camera);

    CameraBlock cb;
    cb.view             = glm::make_mat4(Camera::getViewMatrix(transform).data());
    cb.proj             = glm::make_mat4(Camera::getProjectionMatrix(camData).data());
    cb.viewProj         = cb.proj * cb.view;
    cb.invViewProj      = glm::inverse(cb.viewProj);
    cb.cameraPos        = toGLMVec3(transform.pos);
    cb.fovY             = camData.fovY;
    cb.cameraForward    = toGLMVec3(Camera::getForward(transform));
    cb.aspectRatio      = camData.aspectRatio;    
    cb.cameraRight      = toGLMVec3(Camera::getRight(transform));
    cb.cameraHeight     = getCameraHeightAboveGround(transform.pos);
    cb.cameraUp         = toGLMVec3(Camera::getUp(transform));
    cb.farPlane         = camData.farPlane;
    cb.viewportSize     = glm::vec2(camData.viewportSize.x, camData.viewportSize.y);
    cb.nearPlane        = camData.nearPlane;
    cb._padding         = 0.0f;
    
    EnvironmentBlock eb;
    eb.sunColor         = toGLMVec4(m_astroState.sunColor);
    eb.sunDirection     = toGLMVec3(m_astroState.sunDirection);
    eb.ambientStrength  = m_sunIntensity;
    eb.moonDirection    = toGLMVec3(m_astroState.moonDirection);
    eb.skyExposure      = 5.0f;
    eb.windDirection    = glm::vec2(m_windDirection.x, m_windDirection.y);
    eb.windSpeed        = m_windSpeed;
    eb._padding         = 0.0f;

    AtmosphereBlock ab;
    ab.fogColorDay      = glm::vec4(0.7f, 0.8f, 1.0f, 1.0f);
    ab.fogColorNight    = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f);
    ab.fogDensity       = 0.0002f;
    ab.fogBaseHeight    = 1.0f;
    ab.fogHeightFalloff = 0.005f;
    ab._padding         = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_atmoUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(AtmosphereBlock), &ab);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, m_envUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(EnvironmentBlock), &eb);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraBlock), &cb);
    glBindBuffer(GL_UNIFORM_BUFFER, 0); // safe unbind

    // ==========================================
    // 3. EXECUTE YOUR RENDERING PASSES
    // ==========================================

    glBindFramebuffer(GL_FRAMEBUFFER, m_gBufferFBO);
    glViewport(0, 0, m_gBufferWidth, m_gBufferHeight);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);

    runTerrainPass();
    renderOrbCreature();
    runShadowPass();
    runSSAOPass();
    runSSAOBlurPass();
    renderSky();
    window.clear(sf::Color::Transparent);
    sf::Sprite backgroundSprite(m_skyTexture.getTexture());
    window.draw(backgroundSprite);
    window.setActive(true);
    if (m_debugShowSSAOBlur)
    {
        blitToScreen(m_ssaoPipeline.blurTex);
    } else
    {
        deferredLighting();
    }
    renderOceanGrid();
    window.resetGLStates();
    m_hud->render(window, false);
}

// =========================================================================
// Movement & Physics
// =========================================================================

void Scene_IC_Camp::sMovement(float dt)
{
    for (auto e : m_entityManager.getEntities())
    {
        if (!m_entityManager.hasTransform(e)) continue;
        auto& t = m_entityManager.getTransform(e);

        // 1. Player-specific input handling
        if (m_entityManager.getTag(e) == "player")
        {
            handlePlayerMovement(e, dt);
        }

        // 2. Kinematic movement for entities without physics (orbs, etc.)
        if (!m_entityManager.hasPhysics(e))
        {
            t.pos += t.velocity * dt;
        }

        // 3. Ground resolution + special cases
        resolveEntityPosition(e, dt);
    }

    // SoA-accelerated physics integration for entities with CPhysics
    m_entityManager.forEachPhysics([this, dt](SoAEntityHandle e, CPhysics& p){
        if (!m_entityManager.hasTransform(e)) return;
        auto& t = m_entityManager.getTransform(e);

        // Gravity
        if (!p.onGround)
            t.velocity.y -= p.gravity * dt;

        t.pos += t.velocity * dt;

        // Friction
        float friction = p.onGround ? p.groundFriction : p.airFriction;
        t.velocity.x *= std::max(0.0f, 1.0f - friction * dt);
        t.velocity.z *= std::max(0.0f, 1.0f - friction * dt);
    });
}

void Scene_IC_Camp::handlePlayerMovement(SoAEntityHandle e, float dt)
{
    if (!m_entityManager.hasTransform(e) || !m_entityManager.hasInput(e) ||
        !m_entityManager.hasPhysics(e))
        return;

    auto& t     = m_entityManager.getTransform(e);
    auto& input = m_entityManager.getInput(e);
    auto& phys  = m_entityManager.getPhysics(e);
    // === Locomotion State ===
    phys.isCrouching = input.crouch;
    phys.isSprinting = input.sprint && !phys.isCrouching;

    float moveSpeed = m_playerConfig.MOVE_SPEED;
    if (phys.isSprinting) moveSpeed *= 3.0f;
    else if (phys.isCrouching) moveSpeed *= 0.6f;

    float yawDelta   = -input.mouseDelta.x * 0.002f;
    float pitchDelta = -input.mouseDelta.y * 0.002f;
    
    if (input.strafe)
    {
        if (input.left)  yawDelta -= m_playerConfig.ROTATION_SPEED * dt;
        if (input.right) yawDelta += m_playerConfig.ROTATION_SPEED * dt;
    }
    input.mouseDelta = {0.f, 0.f};

    if (yawDelta != 0.0f)
    {
        glm::quat globalYaw = glm::angleAxis(yawDelta, glm::vec3(0.0f, 1.0f, 0.0f));
        t.setOrientation(globalYaw * t.orientation());
    }

    if (pitchDelta != 0.0f)
    {
        glm::vec3 curForward = t.orientation() * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 localRight = glm::normalize(glm::cross(worldUp, curForward) * -1.0f); 
        glm::quat localPitch = glm::angleAxis(pitchDelta, localRight);
        glm::quat newOrientation = localPitch * t.orientation();
        glm::vec3 testForward = newOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
        if (std::abs(testForward.y) < 0.99f) 
        {
            t.setOrientation(newOrientation);
        }
    }
    
    t.setOrientation(glm::normalize(t.orientation()));
    glm::quat q = t.orientation();
    glm::vec3 f = q * glm::vec3(0.0f, 0.0f, -1.0f);
    
    sf::Vector3f flatForward(f.x, 0.0f, f.z); 
    if (std::sqrt(flatForward.x*flatForward.x + flatForward.z*flatForward.z) > 0.001f)
        flatForward = Camera::normalize(flatForward);

    // === Terrain Info ===
    sf::Vector3f normal = normalAt(t.pos.x, t.pos.z);

    sf::Vector3f terrainForward = flatForward - normal * dot(flatForward, normal);
    if (length(terrainForward) > 0.001f)
        terrainForward = Camera::normalize(terrainForward);
    else
        terrainForward = flatForward;

    sf::Vector3f right = {-terrainForward.z, 0.0f, terrainForward.x};

    // === Slope-based speed modifier ===
    float slopeFactor = 1.0f;
    float slopeCos = dot(terrainForward, normal);
    float slopeAngle = std::acos(std::clamp(slopeCos, -1.0f, 1.0f));

    if (slopeAngle > 0.06f)   // small deadzone
    {
        float grade = std::tan(slopeAngle);

        if (slopeCos < 0.0f) // === DOWNHILL ===
        {
            slopeFactor = 1.0f + std::clamp(grade * 0.65f, 0.0f, 0.22f);
        }
        else // === UPHILL ===
        {
            slopeFactor = 1.0f - std::clamp(grade * 1.1f, 0.0f, 0.18f);
        }
    }

    moveSpeed *= slopeFactor;

    // === Movement Direction ===
    sf::Vector3f moveDir(0.f, 0.f, 0.f);
    if (input.forward)  moveDir += terrainForward;
    if (input.backward) moveDir -= terrainForward;
    if (!input.strafe)
    {
        if (input.left)  moveDir -= right;
        if (input.right) moveDir += right;
    }

    // === Apply Movement ===
    if (length(moveDir) > 0.001f)
    {
        sf::Vector3f desired = Camera::normalize(moveDir) * moveSpeed;

        if (phys.onGround)
        {
            sf::Vector3f desiredOnPlane = desired - normal * dot(desired, normal);
            if (length(desiredOnPlane) > 0.001f)
                desiredOnPlane = Camera::normalize(desiredOnPlane) * moveSpeed;

            t.velocity.x = desiredOnPlane.x;
            t.velocity.z = desiredOnPlane.z;
        }
        else
        {
            const float airControl = 0.25f;
            t.velocity.x = std::lerp(t.velocity.x, desired.x, airControl);
            t.velocity.z = std::lerp(t.velocity.z, desired.z, airControl);
        }
    }
    else if (phys.onGround)
    {
        t.velocity.x = 0.0f;
        t.velocity.z = 0.0f;
        t.velocity.y = 0.0f;
    }

    // === Jumping ===
    if (input.jump && phys.onGround && !phys.isCrouching)
    {
        t.velocity.y = phys.jumpSpeed;
        phys.onGround = false;
    }

    // === Ground handling ===
    const float terrainHeight = heightAt(t.pos.x, t.pos.z);

    if (phys.onGround)
    {
        float heightDiff = t.pos.y - terrainHeight;

        if (heightDiff < -0.2f)
        {
            t.pos.y = terrainHeight;
            t.velocity.y = 0.0f;
        }
        else if (heightDiff > 0.6f)
        {
            phys.onGround = false;
        }
        else
        {
            t.pos.y = std::lerp(t.pos.y, terrainHeight, 0.35f);
            t.velocity.y = std::min(t.velocity.y, 0.0f);
        }
    }
    else
    {
        if (t.velocity.y <= 0.0f && t.pos.y <= terrainHeight + 0.12f)
        {
            t.pos.y = terrainHeight;
            t.velocity.y = 0.0f;
            phys.onGround = true;
        }
    }
}

void Scene_IC_Camp::sGaitAndFootsteps(float dt)
{
    m_entityManager.forEachGaitCycle([this, dt](SoAEntityHandle e, CGaitCycle& gait)
    {
        if (!m_entityManager.hasTransform(e) || !m_entityManager.hasPhysics(e))
            return;

        auto& t    = m_entityManager.getTransform(e);
        auto& phys = m_entityManager.getPhysics(e);

        float horizSpeed = std::sqrt(t.velocity.x*t.velocity.x + t.velocity.z*t.velocity.z);
        float previousPhase = gait.accumulator;

        // === Advance phase (only while actually striding on the ground) ===
        if (phys.onGround && horizSpeed > 0.05f)
        {
            float speedFraction = std::clamp(horizSpeed / m_playerConfig.MOVE_SPEED, 0.0f, 3.0f);
            float rate = gait.strideRate * std::sqrt(speedFraction);
            if (phys.isCrouching) rate *= 0.625f;

            gait.accumulator = std::fmod(gait.accumulator + rate * 60.0f * dt, 1.0f);
        }
        // else: phase holds steady — no striding while airborne or stationary.

        gait.lastPhase = previousPhase;

        // === Footstep events: edge-detect phase crossings ===
        if (horizSpeed > 1.0f && phys.onGround)
        {
            bool shouldStep = false;
            bool isLeft = false;

            if ((previousPhase > 0.8f && gait.accumulator < 0.2f) ||
                (previousPhase < 0.2f && gait.accumulator > 0.8f))
            {
                shouldStep = true; isLeft = true;
            }
            else if ((previousPhase < 0.45f && gait.accumulator >= 0.45f) ||
                     (previousPhase > 0.55f && gait.accumulator <= 0.55f))
            {
                shouldStep = true; isLeft = false;
            }

            if (shouldStep)
            {
                const std::string& soundName = isLeft ? "FootLeft" : "FootRight";
                float volume = phys.isSprinting ? 75.f : (phys.isCrouching ? 30.f : 45.f);
                AudioManager::Instance().sfx.playSound(Assets::Instance().getSound(soundName), volume);
            }
        }
    });
}

void Scene_IC_Camp::resolveEntityPosition(SoAEntityHandle e, float dt)
{
    if (!m_entityManager.hasTransform(e)) return;
    auto& t = m_entityManager.getTransform(e);

    if (m_entityManager.getTag(e) == "orb")
    {
        updateOrbBobbing(e, dt);
        return;
    }

    // Player / other physics entities
    float groundY = heightAt(t.pos.x, t.pos.z);

    if (m_entityManager.hasPhysics(e))
    {
        auto& p = m_entityManager.getPhysics(e);

        const float groundSkin = 0.1f;

        if (t.pos.y < groundY)
        {
            t.pos.y = groundY;
            if (t.velocity.y < 0.0f)
            {
                t.velocity.y = 0.0f;
                p.onGround = true;
            }
        }
        else if (t.pos.y <= groundY + groundSkin)
        {
            p.onGround = true;
        }
        else
        {
            p.onGround = false;
        }
    }
    else
    {
        t.pos.y = groundY;
    }
}

void Scene_IC_Camp::updateBob(SoAEntityHandle e, float dt, float horizSpeed=0.0f)
{
    if (!m_entityManager.hasBob(e) || !m_entityManager.hasTransform(e))
        return;

    auto& bob = m_entityManager.getBob(e);
    auto& t   = m_entityManager.getTransform(e);

    bool isPlayer = m_entityManager.getTag(e) == "player";

    if (isPlayer)
    {
        // Complex player bobbing
        float speedFraction = std::clamp(horizSpeed / m_playerConfig.MOVE_SPEED, 0.0f, 3.0f);
        float baseRate = 0.020f * std::sqrt(speedFraction);

        auto& phys = m_entityManager.getPhysics(e);
        if (phys.isCrouching)
            baseRate *= 0.625f;

        bob.accumulator = std::fmod(bob.accumulator + baseRate * 60.0f * dt, 1.0f);
    }
    else
    {
        // Simple constant-rate bobbing (orbs, etc.)
        bob.accumulator = std::fmod(bob.accumulator + bob.rate * dt, 1.0f);
    }
}

// =========================================================================
// Astronomy & Time Systems
// =========================================================================

void Scene_IC_Camp::updateAstronomySystem()
{
    // The scene remains the master of its data variables, 
    // but the Astro domain handles the heavy lifting instantly.
    m_astroState = Astro::calculateState(
        m_gameYear, 
        m_gameMonth, 
        m_gameDayOfMonth, 
        m_gameTimeOfDay, 
        m_latitude, 
        m_longitude
    );
}

// =========================================================================
// Level & Entity Spawning
// =========================================================================

void Scene_IC_Camp::loadLevel(const std::string& filename)
{
    m_entityManager = EntityManager();

    std::ifstream file(filename);
    std::string str;
    
    while (file >> str)
    {
        if (str == "Terrain")
        {
            std::string manifestPathStr;
            file >> manifestPathStr;
            
            m_terrainStreamer = std::make_unique<TerrainStreamer>(manifestPathStr);
            
            const auto& manifest = m_terrainStreamer->getManifest();
            
            m_latitude  = manifest.worldOriginLatLon.x;
            m_longitude = manifest.worldOriginLatLon.y;
        }
        else if (str == "Camera")
        {
            file >> m_cameraConfig.FOVY
                 >> m_cameraConfig.NEAR_PLANE
                 >> m_cameraConfig.FAR_PLANE;
        }
        else if (str == "Player")
        {
            file >> m_playerConfig.MOVE_SPEED
                 >> m_playerConfig.ROTATION_SPEED
                 >> m_playerConfig.HEIGHT_OFFSET
                 >> m_playerConfig.EYE_OFFSET
                 >> m_playerConfig.POSITION_X
                 >> m_playerConfig.POSITION_Z;
        }
        else if (str == "DateTimePlace")
        {
            // Now only reading time parameters!
            file >> m_gameYear
                 >> m_gameMonth
                 >> m_gameDayOfMonth
                 >> m_gameTimeOfDay;
        }
    }
    
    std::srand(std::time(0));
    m_homeLocationXZ = WCH::hexToWorld(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z);
    m_homeLocation3D = sf::Vector3f(m_homeLocationXZ.x, heightAt(m_homeLocationXZ.x, m_homeLocationXZ.y), m_homeLocationXZ.y);
    m_terrainStreamer->update({m_homeLocation3D.x, m_homeLocation3D.z});
}

void Scene_IC_Camp::spawnPlayer()
{
    m_player = m_entityManager.addEntity("player");
    m_playerConfig.ROTATION_SPEED = Astro::toRad(m_playerConfig.ROTATION_SPEED);
    sf::Vector2f playerPosition = WCH::hexToWorld(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z);
    sf::Vector3f spawnPos(playerPosition.x, heightAt(playerPosition.x, playerPosition.y), playerPosition.y);
    CTransform3D playerTransform(spawnPos);
    playerTransform.setRotation(0.0f, 0.0f, 0.0f);
    m_entityManager.addPlayer(m_player, CPlayer());
    m_entityManager.addTransform(m_player, playerTransform);
    m_entityManager.addInput(m_player, CInput());
    m_entityManager.addPhysics(m_player, CPhysics());
    m_entityManager.addGaitCycle(m_player, CGaitCycle()); // phase, previous phase, step rate, vertical amplitude, lateral amplitude
    auto& phys = m_entityManager.getPhysics(m_player);
    phys.onGround = true;
}

void Scene_IC_Camp::spawnCamera()
{
    m_camera = m_entityManager.addEntity("camera");
    m_cameraConfig.FOVY = Astro::toRad(m_cameraConfig.FOVY);
    CTransform3D cameraTransform;
    cameraTransform.setRotation(0.0f, 0.0f, 0.0f);
    m_entityManager.addCamera(m_camera, CCamera(
        m_cameraConfig.FOVY,
        float(m_cameraConfig.VIEWPORT_WIDTH)/m_cameraConfig.VIEWPORT_HEIGHT,
        m_cameraConfig.NEAR_PLANE,
        m_cameraConfig.FAR_PLANE,
        sf::Vector2u(m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT)
        ));
    m_entityManager.addTransform(m_camera, cameraTransform);
}

void Scene_IC_Camp::spawnOrbFauna(int hexQ, int hexR, float radius,
                                   float bobRate, float bobMagnitude,
                                   const CEyes& eyes,
                                   float yawRad, int species)
{
    const sf::Vector2f groundXZ = WCH::hexToWorld(hexQ, hexR);
    const float        groundY  = heightAt(groundXZ.x, groundXZ.y);
    const float        hoverY   = groundY + radius;

    CTransform3D t(sf::Vector3f(groundXZ.x, hoverY, groundXZ.y));
    
    float yawDeg = yawRad * (180.f / 3.14159265f);
    t.setRotation(0.0f, yawDeg, 0.0f);

    m_entityManager.queueSpawn("orb", [=](SoAEntityHandle orb, EntityManager& em) 
    {
        em.addTransform(orb, t);
        em.addOrb(orb, COrb(radius));
        em.addBob(orb, CBob(bobRate, bobMagnitude, 0.0f));
        em.addEyes(orb, eyes);
        em.getOrb(orb).speciesIdx = species;
    });
}

void Scene_IC_Camp::spawnDebugOrbs(int count)
{
    if (count <= 0) return;

    std::mt19937 rng(1337);

    // Fast 64-bit key for hex positions
    auto posToKey = [](int q, int r) -> uint64_t {
        constexpr int64_t OFFSET = 32768LL;
        return (static_cast<uint64_t>(q + OFFSET) << 32) |
               static_cast<uint64_t>(r + OFFSET);
    };

    std::unordered_set<uint64_t> occupied;

    // Hex cells within distance < 3
    const std::vector<std::pair<int, int>> neighborOffsets = {
        { 0,  0},
        { 1,  0}, { 1, -1}, { 0, -1}, {-1,  0}, {-1,  1}, { 0,  1},
        { 2,  0}, { 2, -1}, { 2, -2}, { 1, -2}, { 0, -2},
        {-1, -1}, {-2,  0}, {-2,  1}, {-2,  2}, {-1,  2}, { 0,  2}, { 1,  1}
    };

    auto isTooClose = [&](int q, int r) -> bool {
        for (const auto& [dq, dr] : neighborOffsets) {
            uint64_t key = posToKey(q + dq, r + dr);
            if (occupied.contains(key)) {
                return true;
            }
        }
        return false;
    };

    // Original distributions
    std::uniform_int_distribution<int> hexDist(-4500, 4500);
    std::uniform_real_distribution<float> radiusDist(0.2f, 1.5f);
    std::uniform_real_distribution<float> bobRateDist(0.2f, 1.0f);
    std::uniform_real_distribution<float> bobMagDist(0.05f, 0.5f);
    std::uniform_real_distribution<float> gazeDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> dilationDist(0.5f, 1.0f);
    std::uniform_real_distribution<float> closureDist(0.0f, 0.0f);
    std::uniform_real_distribution<float> yawDist(0.0f, 2.0f * 3.141592f);
    std::uniform_int_distribution<int> speciesDist(0, 6);

    const int centerQ = m_playerConfig.POSITION_X;
    const int centerR = m_playerConfig.POSITION_Z;

    int spawned = 0;
    int attempts = 0;
    const int maxAttempts = count * 50;

    while (spawned < count && attempts < maxAttempts)
    {
        ++attempts;

        int hexQ = hexDist(rng) + centerQ;
        int hexR = hexDist(rng) + centerR;

        // === NEW: Height filter ===
        sf::Vector2f worldPos = WCH::hexToWorld(hexQ, hexR);
        float height = heightAt(worldPos.x, worldPos.y);

        if (height <= m_seaLevel + 1.5f) {
            continue;
        }

        // Spatial check
        if (isTooClose(hexQ, hexR)) continue;

        // Accept position
        occupied.insert(posToKey(hexQ, hexR));

        float radius = radiusDist(rng);
        float bobRate = bobRateDist(rng);
        float bobMag = bobMagDist(rng);
        float yaw = yawDist(rng);
        int species = speciesDist(rng);

        sf::Vector2f gazeDirection = { gazeDist(rng), gazeDist(rng) };
        float length = std::sqrt(gazeDirection.x * gazeDirection.x + gazeDirection.y * gazeDirection.y);
        if (length > 0.0f) {
            gazeDirection.x /= length;
            gazeDirection.y /= length;
        }

        CEyes eyes;
        eyes.gazeDirection = gazeDirection;
        eyes.pupilDilation = dilationDist(rng);
        eyes.eyelidClosure = closureDist(rng);

        spawnOrbFauna(hexQ, hexR, radius, bobRate, bobMag, eyes, yaw, species);
        ++spawned;
    }
}

// =========================================================================
// Rendering Systems & Pipeline
// =========================================================================

void Scene_IC_Camp::updateCamera(float dt) 
{
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& playerTransform = m_entityManager.getTransform(m_player);
    auto& playerPhysics = m_entityManager.getPhysics(m_player);
    auto& gait = m_entityManager.getGaitCycle(m_player);
    auto& cameraData = m_entityManager.getCamera(m_camera);

    // Smooth crouch transition
    float targetCrouch = playerPhysics.isCrouching ? 1.0f : 0.0f;
    m_crouchFactor += (targetCrouch - m_crouchFactor) * 8.0f * dt;
    m_crouchFactor = std::clamp(m_crouchFactor, 0.0f, 1.0f);

    // Eye height
    float eyeHeight = m_playerConfig.HEIGHT_OFFSET * (1.0f - m_crouchFactor * 0.45f);

    sf::Vector3f headPos = playerTransform.pos + sf::Vector3f(0.f, eyeHeight, 0.f);

    // =========================================================================
    // UPGRADE: Pull direction vectors straight from the player's vector cache
    // =========================================================================
    sf::Vector3f forward = playerTransform.forward();
    sf::Vector3f right   = playerTransform.right();

    float horizontalSpeed = std::sqrt(
        playerTransform.velocity.x * playerTransform.velocity.x +
        playerTransform.velocity.z * playerTransform.velocity.z
    );

    float speedFraction = std::clamp(horizontalSpeed / std::max(m_playerConfig.MOVE_SPEED, 0.0001f), 0.0f, 3.0f);
    float moveFactor = std::clamp(speedFraction, 0.0f, 1.0f);
    float phase = gait.accumulator * 6.2831853f;

    // === Bob Parameters ===
    float baseFrequency = 1.0f;

    float t = std::clamp((speedFraction - 1.0f) / 2.0f, 0.0f, 1.0f); // 0 = walk, 1 = full sprint
    float lateral = gait.lateralMagnitude;
    float vertical = gait.bobMagnitude;
    float lateralAmplitude = lateral - (0.5f * lateral) * t;
    float verticalAmplitude = vertical + (1.0f * vertical) * t;

    if (playerPhysics.isCrouching)
    {
        lateralAmplitude  *= (1.0f - m_crouchFactor * 0.45f);
        verticalAmplitude *= (1.0f - m_crouchFactor * 0.55f);
    }

    lateralAmplitude *= 0.85f;

    float lateralBob  = std::sin(phase * baseFrequency) * lateralAmplitude * moveFactor;
    float verticalBob = std::sin(phase * baseFrequency * 1.65f) * verticalAmplitude * moveFactor;

    sf::Vector3f targetBob = right * lateralBob + sf::Vector3f(0.f, verticalBob, 0.f);
    m_cameraBobOffset += (targetBob - m_cameraBobOffset) * m_bobLag;

    // =========================================================================
    // UPGRADE: Update position, match orientation, and automate the dirty state
    // =========================================================================
    camTransform.pos = headPos - (forward * m_playerConfig.EYE_OFFSET) + m_cameraBobOffset;
    
    // Instead of assigning individual angles, copy the entire rotation basis!
    // If you haven't exposed a direct setter for the underlying quaternion yet, 
    // you can add `void setOrientation(const glm::quat& q) { m_orientation = q; m_isDirty = true; }`
    camTransform.setOrientation(playerTransform.orientation());

    // FOV scales continuously with speed rather than snapping on sprint state
    float targetFov = m_cameraConfig.FOVY + 0.14f * (speedFraction / 3.0f);
    targetFov -= m_crouchFactor * 0.05f;
    cameraData.fovY += (targetFov - cameraData.fovY) * 0.12f;
}

void Scene_IC_Camp::updateHUDData()
{
    auto& playerTransform = m_entityManager.getTransform(m_player);
    sf::Vector3f currentLocation = playerTransform.pos;
    sf::Vector3f forward = forwardFromTransform(playerTransform);
    forward.y = 0.f;
    forward = Camera::normalize(forward);

    float currentHeading = Astro::toDeg(-std::atan2(forward.x, forward.z));

    updateMinimapTexture();

    m_hudData.position = currentLocation;
    m_hudData.mousePos = m_cachedMousePos;
    m_hudData.leftMousePressed = m_leftMousePressed;
    m_hudData.homeLocation = sf::Vector2f(m_homeLocationXZ.x, m_homeLocationXZ.y);
    m_hudData.cameraYaw = currentHeading;
    m_hudData.headlightState = static_cast<int>(m_headlightState);
    m_hudData.headlightEnabled = shouldHeadlightsBeOn();
    m_hudData.minimapTex = &m_minimapTexture.getTexture();
}

void Scene_IC_Camp::buildHud()
{
    m_hud = std::make_unique<HUD>(m_game.window().getSize());
}

bool Scene_IC_Camp::shouldHeadlightsBeOn() const
{
    switch (m_headlightState)
    {
    case HeadlightState::Off:
        return false;
    case HeadlightState::On:
        return true;
    case HeadlightState::Auto:
    default:
        return m_astroState.sunDirection.y < 0.12f || m_sunIntensity < 0.5f;
    }
}

void Scene_IC_Camp::buildTerrainGrid() 
{
    const int W = Topography::GRID_RESOLUTION;
    const int H = Topography::GRID_RESOLUTION;

    std::vector<float> verts;
    verts.reserve(W * H * 2);

    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            float u = col / float(W - 1);
            float v = row / float(H - 1);
            
            verts.push_back(u);
            verts.push_back(v);
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve((W - 1) * (H - 1) * 6);

    for (int row = 0; row < H - 1; ++row) {
        for (int col = 0; col < W - 1; ++col) {
            unsigned int tl = row * W + col;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + W;
            unsigned int br = bl + 1;
            
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    m_gridIndexCount = static_cast<GLuint>(indices.size());

    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);
    glGenBuffers(1, &m_gridEBO);

    glBindVertexArray(m_gridVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gridEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Scene_IC_Camp::buildVertexCube()
{
    static const float cubeVerts[] = {
        -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1, // back face
        -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1, // front face
    };

    static const unsigned int cubeIndices[] = {
        0,1,2,  2,3,0, // back
        4,5,6,  6,7,4, // front
        0,4,7,  7,3,0, // left
        1,5,6,  6,2,1, // right
        3,7,6,  6,2,3, // top
        0,4,5,  5,1,0  // bottom
    };

    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glGenBuffers(1, &m_cubeEBO);

    glBindVertexArray(m_cubeVAO);

    // Vertex positions
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,          // location = 0 in your vert shader
        3,          // vec3
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),  // tightly packed
        (void*)0
    );
    glEnableVertexAttribArray(0);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Scene_IC_Camp::generateOceanMesh(float size, int resolution, unsigned int& vao, unsigned int& vbo, unsigned int& ebo, unsigned int& indexCount)
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    float halfSize = size / 2.0f;
    float step = size / (float)resolution;

    // 1. Generate vertices (Positions, Normals, TexCoords)
    for (int z = 0; z <= resolution; z++)
    {
        for (int x = 0; x <= resolution; x++)
        {
            float posX = -halfSize + (float)x * step;
            float posZ = -halfSize + (float)z * step;
            float posY = 0.0f; // Flat to begin with

            // Position
            vertices.push_back(posX);
            vertices.push_back(posY);
            vertices.push_back(posZ);

            // Normal (pointing straight up initially)
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);

            // TexCoords
            vertices.push_back((float)x / (float)resolution);
            vertices.push_back((float)z / (float)resolution);
        }
    }

    // 2. Generate indices for triangles
    for (int z = 0; z < resolution; z++)
    {
        for (int x = 0; x < resolution; x++)
        {
            unsigned int topLeft     = z * (resolution + 1) + x;
            unsigned int topRight    = topLeft + 1;
            unsigned int bottomLeft  = (z + 1) * (resolution + 1) + x;
            unsigned int bottomRight = bottomLeft + 1;

            // Triangle 1
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Triangle 2
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    indexCount = static_cast<unsigned int>(indices.size());

    // 3. Bind buffers to OpenGL
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Vertex Positions (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    // Vertex Normals (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    // Vertex Texture Coordinates (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

void Scene_IC_Camp::initializeSkyCubemap()
{

    m_skyCubemapHandle = Assets::Instance().getCubemap("NightSky");
    m_skyCubemapReady  = (m_skyCubemapHandle != 0);

    if (!m_skyCubemapReady)
        std::cerr << "Sky cubemap 'NightSky' not found in Assets; falling back to procedural sky." << std::endl;
}

void Scene_IC_Camp::initializeShadowMap(unsigned int size) 
{
    m_shadowMapSize = size;

    glGenTextures(1, &m_shadowDepthTexArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowDepthTexArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 size, size, NUM_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &m_shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::destroyShadowMap() 
{
    if (m_shadowFBO) {
        glDeleteFramebuffers(1, &m_shadowFBO);
        glDeleteTextures(1, &m_shadowDepthTexArray);
        m_shadowFBO = 0;
        m_shadowDepthTexArray = 0;
    }
}

float Scene_IC_Camp::computeDistanceToMapFarCorner(const glm::vec3& camPos) const 
{
    glm::vec2 camPosXZ(camPos.x, camPos.z);
    glm::vec2 mapMin(m_topdownWorldMin.x, m_topdownWorldMin.y);
    glm::vec2 mapMax = mapMin + glm::vec2(m_topdownWorldSize.x, m_topdownWorldSize.y);

    glm::vec2 corners[4] = {
        { mapMin.x, mapMin.y }, { mapMax.x, mapMin.y },
        { mapMin.x, mapMax.y }, { mapMax.x, mapMax.y }
    };

    float maxDist = 0.0f;
    for (auto& c : corners) {
        maxDist = std::max(maxDist, glm::length(c - camPosXZ));
    }
    return maxDist;
}

void Scene_IC_Camp::computeCascadeSplits(float camNear, float camFar) 
{
    for (int i = 0; i < NUM_CASCADES; ++i) {
        float p = float(i + 1) / float(NUM_CASCADES);
        float logSplit     = camNear * std::pow(camFar / camNear, p);
        float uniformSplit = camNear + (camFar - camNear) * p;
        m_cascadeSplits[i] = m_cascadeSplitLambda * logSplit
                           + (1.0f - m_cascadeSplitLambda) * uniformSplit;
    }

    auto& camTransform = m_entityManager.getTransform(m_camera);
    float mapEdgeDist  = computeDistanceToMapFarCorner(toGLMVec3(camTransform.pos));

    m_cascadeSplits[NUM_CASCADES - 1] =
        std::max(mapEdgeDist, m_cascadeSplits[NUM_CASCADES - 2] + 1.0f);
}

glm::mat4 Scene_IC_Camp::computeLightViewProjForRange(float splitNear, float splitFar, float& lightDepthRange, float& texelWorldSize) const 
{
    glm::vec3 sunDir  = glm::normalize(toGLMVec3(m_astroState.sunDirection));
    glm::vec3 worldUp = (std::abs(sunDir.y) > 0.99f) ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -sunDir, worldUp);

    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData      = m_entityManager.getCamera(m_camera);

    // Call your native custom tracking code cleanly
    sf::Vector3f centroidSF;
    float radius;
    Camera::getFrustumBoundingSphere(camTransform, camData, splitNear, splitFar, centroidSF, radius);
    radius += m_shadowFrustumPadding;

    glm::vec3 centroid = glm::vec3(centroidSF.x, centroidSF.y, centroidSF.z);

    // Snapping logic remains exactly the same
    float worldUnitsPerTexel = (radius * 2.0f) / float(m_shadowMapSize);
    texelWorldSize = worldUnitsPerTexel; 

    glm::vec3 centroidLS = glm::vec3(lightView * glm::vec4(centroid, 1.0f));
    centroidLS.x = std::floor(centroidLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    centroidLS.y = std::floor(centroidLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    float nearExtension = m_shadowFrustumPadding * 20.0f;
    centroidLS.z += radius + nearExtension;

    glm::vec3 eyeWorldPos    = glm::vec3(glm::inverse(lightView) * glm::vec4(centroidLS, 1.0f));
    glm::mat4 finalLightView = glm::lookAt(eyeWorldPos, eyeWorldPos - sunDir, worldUp);

    float farDist   = radius * 2.0f + nearExtension;
    lightDepthRange = farDist;

    // Fetch the raw flat orthographic matrix array and cast to GLM instantly via make_mat4
    std::array<float, 16> orthoProjRaw = Camera::getOrthoMatrix(-radius, radius, -radius, radius, 0.0f, farDist);
    return glm::make_mat4(orthoProjRaw.data()) * finalLightView;
}

glm::mat4 Scene_IC_Camp::computeLightViewProjForMapBounds(float& lightDepthRange, float& texelWorldSize) const 
{
    glm::vec3 sunDir  = glm::normalize(toGLMVec3(m_astroState.sunDirection));
    glm::vec3 worldUp = (std::abs(sunDir.y) > 0.99f) ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -sunDir, worldUp);

    glm::vec2 mapMin(m_topdownWorldMin.x, m_topdownWorldMin.y);
    glm::vec2 mapMax(mapMin.x + m_topdownWorldSize.x, mapMin.y + m_topdownWorldSize.y);
    float heightMin = 0.0f;
    float heightMax = m_topdownMaxHeight;

    glm::vec3 worldCorners[8] = {
        { mapMin.x, heightMin, mapMin.y }, { mapMax.x, heightMin, mapMin.y },
        { mapMax.x, heightMin, mapMax.y }, { mapMin.x, heightMin, mapMax.y },
        { mapMin.x, heightMax, mapMin.y }, { mapMax.x, heightMax, mapMin.y },
        { mapMax.x, heightMax, mapMax.y }, { mapMin.x, heightMax, mapMax.y },
    };

    glm::vec3 centroid(0.0f);
    for (auto& c : worldCorners) centroid += c;
    centroid /= 8.0f;

    float radius = 0.0f;
    for (auto& c : worldCorners) {
        radius = std::max(radius, glm::length(c - centroid));
    }
    radius += m_shadowFrustumPadding;

    float worldUnitsPerTexel = (radius * 2.0f) / float(m_shadowMapSize);
    texelWorldSize = worldUnitsPerTexel;

    glm::vec3 centroidLS = glm::vec3(lightView * glm::vec4(centroid, 1.0f));
    centroidLS.x = std::floor(centroidLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    centroidLS.y = std::floor(centroidLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    float nearExtension = m_shadowFrustumPadding * 20.0f;
    centroidLS.z += radius + nearExtension;

    glm::vec3 eyeWorldPos    = glm::vec3(glm::inverse(lightView) * glm::vec4(centroidLS, 1.0f));
    glm::mat4 finalLightView = glm::lookAt(eyeWorldPos, eyeWorldPos - sunDir, worldUp);

    float farDist   = radius * 2.0f + nearExtension;
    lightDepthRange = farDist;

    std::array<float, 16> orthoProjRaw = Camera::getOrthoMatrix(-radius, radius, -radius, radius, 0.0f, farDist);
    return glm::make_mat4(orthoProjRaw.data()) * finalLightView;
}

void Scene_IC_Camp::runShadowPass() 
{
    auto& camData = m_entityManager.getCamera(m_camera);
    
    // 1. Calculate cascade distributions based on our camera properties
    float clampedFar = std::min(camData.farPlane, m_shadowMaxDistance);
    computeCascadeSplits(camData.nearPlane, clampedFar);

    // 2. Setup Render State for Shadow Maps
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glViewport(0, 0, m_shadowMapSize, m_shadowMapSize);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE); // Shared by both terrain and orbs here

    float splitNear = camData.nearPlane;

    for (int cascade = 0; cascade < NUM_CASCADES; ++cascade) {
        float splitFar = m_cascadeSplits[cascade];

        // Compute the light space matrices for this slice
        if (cascade == NUM_CASCADES - 1) {
            m_lightViewProjCascades[cascade] = computeLightViewProjForMapBounds(
                m_lightDepthRange[cascade], m_texelWorldSize[cascade]);
        } else {
            m_lightViewProjCascades[cascade] = computeLightViewProjForRange(
                splitNear, splitFar, m_lightDepthRange[cascade], m_texelWorldSize[cascade]);
        }
        splitNear = splitFar; // Next cascade picks up where this one left off

        // Target the specific layer array slice on the FBO
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   m_shadowDepthTexArray, 0, cascade);
        glClear(GL_DEPTH_BUFFER_BIT);

        // --- 1. TERRAIN SHADOWS ---
        glUseProgram(m_terrainShadowProgram);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainStreamer->getOrUploadArrayTexture());
        glUniform1i(Uniforms::SharedTerrain::TerrainHeightArray, 0);

        sf::Vector2f gridOrigin = m_terrainStreamer->getVisibleGridWorldOrigin();
        glUniform2f(Uniforms::SharedTerrain::TerrainGridWorldOrigin,
                    gridOrigin.x, gridOrigin.y);

        constexpr float kTileWorldSize =
            WorldCoordinates::Square::kTexelSizeM * WorldCoordinates::Square::kTileResolution;
        glUniform1f(Uniforms::SharedTerrain::TerrainTileWorldSize, kTileWorldSize);

        glUniform1iv(Uniforms::SharedTerrain::TerrainSliceValid, 81,
                    m_terrainStreamer->getActiveSliceUniforms().data());
        
        glUniformMatrix4fv(Uniforms::ShadowPass::LightViewProj,
                           1, GL_FALSE, &m_lightViewProjCascades[cascade][0][0]);
        
        glBindVertexArray(m_gridVAO);
        glDrawElements(GL_TRIANGLES, m_gridIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // --- 2. ORB SHADOWS (Skip stable far bounds map layer if configured) ---
        if (cascade != NUM_CASCADES - 1)
        {
            glUseProgram(m_orbShadowProgram);
            
            glUniformMatrix4fv(Uniforms::ShadowPass::LightViewProj,
                            1, GL_FALSE, &m_lightViewProjCascades[cascade][0][0]);
            
            m_orbSSBO.bind(0);
            glBindVertexArray(m_cubeVAO);
            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, m_orbSSBO.count());
            glBindVertexArray(0);
        }
    }

    // 3. Reset Global Pipeline State
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::initSSAOKernel() 
{
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator;

    m_ssaoKernel.clear();
    for (unsigned int i = 0; i < m_ssaoKernelSize; ++i) {
        // 1. Generate random points in a hemisphere (z goes from 0.0 to 1.0)
        sf::Glsl::Vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        
        // Normalize to push them to the edge of the hemisphere
        // (Assuming a basic normalization helper, or do it manually)
        float len = std::sqrt(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z);
        if (len > 0.0f) { sample.x /= len; sample.y /= len; sample.z /= len; }

        // 2. Scale the samples so they bunch up closer to the origin 
        // This gives better close-range occlusion details
        float scale = (float)i / (float)m_ssaoKernelSize;
        // Accelerating interpolation math: lerp(0.1f, 1.0f, scale^2)
        scale = 0.1f + (scale * scale) * (1.0f - 0.1f); 
        
        sample.x *= scale;
        sample.y *= scale;
        sample.z *= scale;

        m_ssaoKernel.push_back(sample);
    }
}

void Scene_IC_Camp::initSSAOFramebuffers() 
{
    // -------------------------------------------------------------------------
    // 1. RAW SSAO FRAMEBUFFER
    // -------------------------------------------------------------------------
    glGenFramebuffers(1, &m_ssaoPipeline.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoPipeline.fbo);

    glGenTextures(1, &m_ssaoPipeline.colorTex);
    glBindTexture(GL_TEXTURE_2D, m_ssaoPipeline.colorTex);
    
    // SSAO only needs a single grayscale channel. GL_RED or GL_R8 is perfect and fast.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_gBufferWidth, m_gBufferHeight, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Attach texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoPipeline.colorTex, 0);

    // Verify FBO is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "SSAO Framebuffer not complete!" << std::endl;
    }

    // -------------------------------------------------------------------------
    // 2. SSAO BLUR FRAMEBUFFER
    // -------------------------------------------------------------------------
    glGenFramebuffers(1, &m_ssaoPipeline.blurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoPipeline.blurFBO);

    glGenTextures(1, &m_ssaoPipeline.blurTex);
    glBindTexture(GL_TEXTURE_2D, m_ssaoPipeline.blurTex);
    
    // Same single-channel setup for the blurred result
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_gBufferWidth, m_gBufferHeight, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoPipeline.blurTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    }

    // Restore default framebuffer binding
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::initUBOs() 
{
    // 1. Generate the buffer object
    glGenBuffers(1, &m_cameraUBO);
    
    // 2. Bind it as a Uniform Buffer
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    
    // 3. Allocate memory space for the struct (null pointer means allocate space but don't copy yet)
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraBlock), nullptr, GL_DYNAMIC_DRAW);
    
    // 4. Unbind to keep state clean
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // 5. Explicitly link this buffer handle to global Uniform Binding Slot 0
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);

    glGenBuffers(1, &m_envUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_envUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(EnvironmentBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Link to Slot 1
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_envUBO);

    glGenBuffers(1, &m_atmoUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_atmoUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(AtmosphereBlock), nullptr, GL_STATIC_DRAW); // Or GL_DYNAMIC_DRAW if you update dynamically
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_atmoUBO);
}

void Scene_IC_Camp::initSSAONoiseTexture() 
{
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator;

    std::vector<sf::Glsl::Vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; ++i) {
        // Rotate around the Z-axis (z is 0.0)
        sf::Glsl::Vec3 noise(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f
        );
        ssaoNoise.push_back(noise);
    }

    // Generate the OpenGL texture
    glGenTextures(1, &m_ssaoPipeline.noiseTex);
    glBindTexture(GL_TEXTURE_2D, m_ssaoPipeline.noiseTex);
    
    // Upload raw float data (3 channels: RGB)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0].x);
    
    // CRITICAL: Set wrapping to repeat so it tiles across the full screen dimensions
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Scene_IC_Camp::uploadViewRays(GLuint shaderProgram) 
{
    // Grab your camera properties
    auto cameraData = m_entityManager.getCamera(m_camera);
    float fov = cameraData.fovY; // in degrees
    float aspect = cameraData.aspectRatio; // width / height
    float farPlane = cameraData.farPlane;

    // Calculate half-width and half-height of the frustum at the far plane
    float tangent = std::tan(fov * 0.5f * (3.14159265f / 180.0f));
    float farHeight = farPlane * tangent;
    float farWidth = farHeight * aspect;

    glUniform3f(Uniforms::SSAO::TopRight,   farWidth,  farHeight, -farPlane);
    glUniform3f(Uniforms::SSAO::TopLeft,   -farWidth,  farHeight, -farPlane);
    glUniform3f(Uniforms::SSAO::BottomLeft, -farWidth, -farHeight, -farPlane);
    glUniform3f(Uniforms::SSAO::BottomRight, farWidth, -farHeight, -farPlane);
}

void Scene_IC_Camp::runSSAOPass() 
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoPipeline.fbo);
    glViewport(0, 0, m_gBufferWidth, m_gBufferHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(m_ssao);
    uploadViewRays(m_ssao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glUniform1i(Uniforms::GBuffer::GNormalTex, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gDepthTex);
    glUniform1i(Uniforms::GBuffer::GDepthTex, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_ssaoPipeline.noiseTex);
    glUniform1i(Uniforms::SSAO::NoiseTex, 2);

    // 2. Upload Kernel & Structural Parameters
    glUniform3fv(Uniforms::SSAO::KernelSample, static_cast<GLsizei>(m_ssaoKernel.size()), &m_ssaoKernel[0].x);
    glUniform2f(Uniforms::SSAO::NoiseScale, (float)m_gBufferWidth / 4.0f, (float)m_gBufferHeight / 4.0f);
    glUniform1f(Uniforms::SSAO::Radius, m_debugSSAOKernelRadius);
    glUniform1f(Uniforms::SSAO::Bias, m_debugSSAOBias);
    glUniform1i(Uniforms::SSAO::SampleCount, static_cast<GLint>(m_sampleCount));

    // 3. Procedural Screen-Space Triangle Draw Call (Zero allocation overhead)
    glBindVertexArray(m_gridVAO); // Reusing your existing valid VAO
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::runSSAOBlurPass() 
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoPipeline.blurFBO);
    glViewport(0, 0, m_gBufferWidth, m_gBufferHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_ssao_blur);

    // 1. Bind Input Texture Contexts
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_ssaoPipeline.colorTex);
    glUniform1i(Uniforms::SSAOBlur::SSAOInput, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glUniform1i(Uniforms::GBuffer::GNormalTex, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_gDepthTex);
    glUniform1i(Uniforms::GBuffer::GDepthTex, 2);

    // 2. Procedural Fullscreen Draw Call (Zero allocation overhead)
    glBindVertexArray(m_gridVAO); 
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::initializeGBuffer(unsigned int width, unsigned int height) 
{
    m_gBufferWidth = width;
    m_gBufferHeight = height;

    // 1. Create FBO
    glGenFramebuffers(1, &m_gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_gBufferFBO);

    // Helper lambda to create generic color textures
    auto createColorTex = [&](GLuint& tex, GLenum internalFormat, GLenum attachment) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, 
                    (internalFormat == GL_RGBA16F ? GL_FLOAT : GL_UNSIGNED_BYTE), nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);
    };

    // 2. Provision layout attachments
    createColorTex(m_gAlbedoTex,  GL_RGBA8,   GL_COLOR_ATTACHMENT0); // GB0
    createColorTex(m_gNormalTex,  GL_RGBA16F, GL_COLOR_ATTACHMENT1); // GB1
    createColorTex(m_gIndicesTex, GL_RGBA8,   GL_COLOR_ATTACHMENT2); // GB2
    createColorTex(m_gRetroTex,   GL_RGBA8,   GL_COLOR_ATTACHMENT3); // GB3

    // 3. Depth Texture (D32F)
    glGenTextures(1, &m_gDepthTex);
    glBindTexture(GL_TEXTURE_2D, m_gDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gDepthTex, 0);

    // 4. Declare array of color buffers to draw to
    GLenum drawBuffers[] = { 
        GL_COLOR_ATTACHMENT0, 
        GL_COLOR_ATTACHMENT1, 
        GL_COLOR_ATTACHMENT2, 
        GL_COLOR_ATTACHMENT3 
    };
    glDrawBuffers(4, drawBuffers);

    // 5. Verify status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "G-Buffer FBO incomplete! Status: 0x" << std::hex 
                << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::destroyGBuffer() 
{
    if (m_gBufferFBO) {
        glDeleteFramebuffers(1, &m_gBufferFBO);
        glDeleteTextures(1, &m_gAlbedoTex);
        glDeleteTextures(1, &m_gNormalTex);
        glDeleteTextures(1, &m_gIndicesTex);
        glDeleteTextures(1, &m_gRetroTex);
        glDeleteTextures(1, &m_gDepthTex);

        m_gBufferFBO   = 0;
        m_gAlbedoTex   = 0;
        m_gNormalTex   = 0;
        m_gIndicesTex  = 0;
        m_gRetroTex    = 0;
        m_gDepthTex    = 0;
    }
}

std::vector<OrbData> Scene_IC_Camp::buildOrbData() const
{
    std::vector<OrbData> orbData;
    orbData.reserve(m_entityManager.getEntities("orb").size());

    for (auto& orb : m_entityManager.getEntities("orb"))
    {
        auto& t    = m_entityManager.getTransform(orb);
        auto& c    = m_entityManager.getOrb(orb);
        auto& eyes = m_entityManager.getEyes(orb);
        const auto& f = t.forward();
        const auto& r = t.right();
        const auto& u = t.up();

        OrbData data;
        data.centreAndSpeciesIdx                = { t.pos.x, t.pos.y, t.pos.z, static_cast<float>(c.speciesIdx) };
        data.forwardAndRadius                   = { f.x, f.y, f.z, c.radius };
        data.rightPadded                        = { r.x, r.y, r.z, 0.0f };
        data.upPadded                           = { u.x, u.y, u.z, 0.0f };
        data.gazeDirDilationAndEyelidClosure    = { eyes.gazeDirection.x, eyes.gazeDirection.y, eyes.pupilDilation, eyes.eyelidClosure };
        orbData.push_back(data);
    }
    return orbData;
}

void Scene_IC_Camp::initializeOrbShaderStorage()
{
    m_orbStagingBuffer.reserve(MAX_ORB_CAPACITY);
    updateOrbShaderStorage();
}

void Scene_IC_Camp::updateOrbShaderStorage()
{
    m_orbStagingBuffer.clear();

    const auto& orbEntities = m_entityManager.getEntities("orb");

    for (const auto& orb : orbEntities)
    {
        if (m_orbStagingBuffer.size() >= MAX_ORB_CAPACITY) break;

        const auto& t    = m_entityManager.getTransform(orb);
        const auto& c    = m_entityManager.getOrb(orb);
        const auto& eyes = m_entityManager.getEyes(orb);
        
        const auto& f = t.forward();
        const auto& r = t.right();
        const auto& u = t.up();

        m_orbStagingBuffer.push_back(OrbData{
            .centreAndSpeciesIdx = { t.pos.x, t.pos.y, t.pos.z, static_cast<float>(c.speciesIdx) },
            .forwardAndRadius    = { f.x, f.y, f.z, c.radius },
            .rightPadded         = { r.x, r.y, r.z, 0.0f },
            .upPadded            = { u.x, u.y, u.z, 0.0f },
            .gazeDirDilationAndEyelidClosure = { eyes.gazeDirection.x, eyes.gazeDirection.y, eyes.pupilDilation, eyes.eyelidClosure }
        });
    }

    m_orbSSBO.update(m_orbStagingBuffer.data(), m_orbStagingBuffer.size());
}

void Scene_IC_Camp::runTerrainPass() 
{
    sf::Vector2i hex(-1, -1);
    if (m_cursorMode) {
        sf::Vector3f worldPos = screenToWorld(m_cachedMousePos);
        hex = WCH::worldToHex(worldPos.x, worldPos.z);
    }

    glUseProgram(m_terrainProgram);

    // 1. Bind Material SSBO & Texture Arrays
    Assets::Instance().getMaterialSSBO().bind(1); // Matches layout(std430, binding = 1) in GLSL

    // Unit 0: Height Map Array
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainStreamer->getOrUploadArrayTexture());
    glUniform1i(Uniforms::SharedTerrain::TerrainHeightArray, 0);

    // Unit 1: Diffuse Splat Array
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Assets::Instance().getTerrainDiffuseArray());
    glUniform1i(Uniforms::Terrain::TerrainDiffuseArray, 1);

    // Unit 2: Normal Splat Array
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Assets::Instance().getTerrainNormalArray());
    glUniform1i(Uniforms::Terrain::TerrainNormalArray, 2);

    // Unit 3: Road SDF Distance Field Array (ADDED)
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainStreamer->getOrUploadRoadArrayTexture());
    glUniform1i(Uniforms::SharedTerrain::TerrainRoadArray, 3);

    // 2. Terrain Streaming Uniforms
    sf::Vector2f gridOrigin = m_terrainStreamer->getVisibleGridWorldOrigin();
    glUniform2f(Uniforms::SharedTerrain::TerrainGridWorldOrigin, gridOrigin.x, gridOrigin.y);

    constexpr float kTileWorldSize =
        WorldCoordinates::Square::kTexelSizeM * WorldCoordinates::Square::kTileResolution;
    glUniform1f(Uniforms::SharedTerrain::TerrainTileWorldSize, kTileWorldSize);

    glUniform1iv(Uniforms::SharedTerrain::TerrainSliceValid, 81,
                m_terrainStreamer->getActiveSliceUniforms().data());

    // 3. Structural & Geometry Tweak Uniforms
    glUniform1f(Uniforms::SharedTerrain::HeightMax, m_topdownMaxHeight);
    glUniform1f(Uniforms::Terrain::ReliefExaggeration, 1.0f);
    glUniform1f(Uniforms::Terrain::SeaLevel, m_seaLevel);

    // 4. Cursor & Selection Grid Uniforms
    glUniform1i(Uniforms::Terrain::CursorMode, static_cast<int>(m_cursorMode));
    glUniform1f(Uniforms::Terrain::HexSize, m_hexSize);
    glUniform2f(Uniforms::Terrain::HoveredHex, static_cast<float>(hex.x), static_cast<float>(hex.y));
    glUniform3f(Uniforms::Terrain::GridColour, m_gridColour.r / 255.f, m_gridColour.g / 255.f, m_gridColour.b / 255.f);
    glUniform1i(Uniforms::Terrain::DrawHexGrid, static_cast<int>(m_drawHexGrid));
    
    // 5. Render
    glBindVertexArray(m_gridVAO);
    glDrawElements(GL_TRIANGLES, m_gridIndexCount, GL_UNSIGNED_INT, 0);

    // 6. Cleanup
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D_ARRAY, 0); // Cleanup Unit 3
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glUseProgram(0);
}

void Scene_IC_Camp::updateMinimapTexture()
{
    if (m_minimapTexture.getSize().x != m_minimapTextureSize ||
        m_minimapTexture.getSize().y != m_minimapTextureSize)
    {
        m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});
    }

    const sf::Vector3f playerPos = m_entityManager.getTransform(m_player).pos;
    const float texSize  = static_cast<float>(m_minimapTextureSize);  // 256
    const float center   = texSize * 0.5f;
    const float worldRadius = 2560.f;

    m_minimapTexture.clear(sf::Color::Transparent);
    m_minimapTexture.setActive(true); // Directs OpenGL context to this target

    // --- 1. SAVE SFML STATES ---
    m_minimapTexture.pushGLStates();

    // Reset viewport to match the texture size
    glViewport(0, 0, m_minimapTextureSize, m_minimapTextureSize);

    glUseProgram(m_minimapProgram);

    // Uniforms
    glUniform2f(Uniforms::MiniMap::PlayerXZ, playerPos.x, playerPos.z);
    glUniform1f(Uniforms::MiniMap::WorldRadius, worldRadius);
    glUniform1f(Uniforms::MiniMap::SeaLevel, m_seaLevel);
    glUniform1f(Uniforms::SharedTerrain::HeightMax, m_topdownMaxHeight);

    // Bind the terrain height array texture safely to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainStreamer->getOrUploadArrayTexture());
    glUniform1i(Uniforms::SharedTerrain::TerrainHeightArray, 0);

    sf::Vector2f gridOrigin = m_terrainStreamer->getVisibleGridWorldOrigin();
    glUniform2f(Uniforms::SharedTerrain::TerrainGridWorldOrigin, gridOrigin.x, gridOrigin.y);

    constexpr float kTileWorldSize =
        WorldCoordinates::Square::kTexelSizeM * WorldCoordinates::Square::kTileResolution;
    glUniform1f(Uniforms::SharedTerrain::TerrainTileWorldSize, kTileWorldSize);

    glUniform1iv(Uniforms::SharedTerrain::TerrainSliceValid, 81, m_terrainStreamer->getActiveSliceUniforms().data());

    // Draw fullscreen triangle using the zero-attribute VAO trick
    glBindVertexArray(m_gridVAO); 
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Unbind shader and texture to clean up raw GL states
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glUseProgram(0);

    // --- 2. RESTORE SFML STATES ---
    m_minimapTexture.popGLStates();

    // Now SFML's 2D drawer can safely draw the home marker without state corruption!
    sf::Vector2f delta = m_homeLocationXZ - sf::Vector2f(playerPos.x, playerPos.z);
    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const float circleRadiusPx = center - 2.f;
    const float homeMarkerRadius = 5.5f;
    const float maxMarkerDist = std::max(0.f, circleRadiusPx - homeMarkerRadius - 1.f);
    const float worldToPixel = circleRadiusPx / worldRadius;

    if (dist > 0.0001f)
    {
        sf::Vector2f homePx = sf::Vector2f(center, center) + delta * worldToPixel;
        sf::Vector2f diff = homePx - sf::Vector2f(center, center);
        float markerDist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (markerDist > maxMarkerDist)
        {
            sf::Vector2f dir = diff / markerDist;
            homePx = sf::Vector2f(center, center) + dir * maxMarkerDist;
        }

        sf::CircleShape homeHex(homeMarkerRadius, 6);
        homeHex.setOrigin({homeMarkerRadius, homeMarkerRadius});
        homeHex.setPosition(homePx);
        homeHex.setFillColor(sf::Color(245, 225, 98, 255));
        homeHex.setOutlineThickness(1.f);
        homeHex.setOutlineColor(sf::Color(88, 70, 24, 240));
        
        m_minimapTexture.draw(homeHex);
    }

    m_minimapTexture.display();
}

void Scene_IC_Camp::renderOrbCreature()
{
    // ==================== LAZY INIT ====================
    if (m_cubeVAO == 0)
        buildVertexCube();

    // ==================== PROGRAM ====================
    glUseProgram(m_OrbCreatureProgram);

    // ==================== TEXTURES ====================
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Assets::Instance().getSpeciesDiffuseArray());
    glUniform1i(Uniforms::OrbCreature::DiffuseTex, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Assets::Instance().getSpeciesNormalArray());
    glUniform1i(Uniforms::OrbCreature::NormalTex, 1);

    // ==================== DRAW ====================
    m_orbSSBO.bind(0);
    Assets::Instance().getSpeciesSSBO().bind(1);
    glBindVertexArray(m_cubeVAO);
    glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, m_orbSSBO.count());

    // ==================== CLEANUP ====================
    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void Scene_IC_Camp::deferredLighting()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    sf::Vector2u windowSize = m_game.window().getSize();
    glViewport(0, 0, windowSize.x, windowSize.y);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_lightingProgram);

    if (m_lightingVAO == 0) {
        static const float quadVerts[] = {
            -1.f, -1.f,  1.f, -1.f,  1.f,  1.f,
            -1.f, -1.f,  1.f,  1.f, -1.f,  1.f,
        };
        glGenVertexArrays(1, &m_lightingVAO);
        glGenBuffers(1, &m_lightingVBO);
        glBindVertexArray(m_lightingVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_lightingVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // =========================================================================
    // Bind G-Buffer Textures to Texture Units
    // =========================================================================
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gAlbedoTex);
    glUniform1i(Uniforms::GBuffer::GAlbedoTex, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glUniform1i(Uniforms::GBuffer::GNormalTex, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_gIndicesTex);
    glUniform1i(Uniforms::GBuffer::GIndicesTex, 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_gRetroTex);
    glUniform1i(Uniforms::GBuffer::GRetroTex, 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_gDepthTex);
    glUniform1i(Uniforms::GBuffer::GDepthTex, 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowDepthTexArray);
    glUniform1i(Uniforms::Shadows::ShadowMapArray, 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_ssaoPipeline.blurTex);
    glUniform1i(Uniforms::Lighting::SSAOTex, 6);

    // =========================================================================
    // Forward Light Projection & Cascade Specifics (Still Loose Uniforms)
    // =========================================================================
    glUniformMatrix4fv(Uniforms::Shadows::LightViewProj, NUM_CASCADES, GL_FALSE, &m_lightViewProjCascades[0][0][0]);
    glUniform1fv(Uniforms::Shadows::TexelWorldSize, NUM_CASCADES, m_texelWorldSize);
    glUniform1fv(Uniforms::Shadows::CascadeSplitDepths, NUM_CASCADES, m_cascadeSplits);
    // =========================================================================
    // Bind SSBOs (Safely living up on non-conflicting slots 5 & 6)
    // =========================================================================
    Assets::Instance().getSpeciesSSBO().bind(5);
    Assets::Instance().getMaterialSSBO().bind(6);

    // =========================================================================
    // Forward Player Specific Context State
    // =========================================================================
    glUniform3fv(Uniforms::Lighting::NightAmbientFloor, 1, &m_nightAmbientFloor[0]);
    glUniform1f(Uniforms::Lighting::HeadlampIntensity, 2.0f);
    glUniform1f(Uniforms::Lighting::HeadlampRange,     200.0f);
    glUniform1f(Uniforms::Lighting::HeadlampEnabled,   shouldHeadlightsBeOn() ? 1.0f : 0.0f);

    // =========================================================================
    // Draw Fullscreen Composition Quad
    // =========================================================================
    glBindVertexArray(m_lightingVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ==================== CLEANUP ====================
    glBindVertexArray(0);
    glUseProgram(0);
    
    for (int i = 0; i < 7; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    glActiveTexture(GL_TEXTURE0); 
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::blitToScreen(GLuint tex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    sf::Vector2u windowSize = m_game.window().getSize();
    glViewport(0, 0, windowSize.x, windowSize.y);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(Uniforms::Blit::InputTex, 0);

    if (m_blitVAO == 0) {
        static const float quadVerts[] = {
            -1.f, -1.f,
             1.f, -1.f,
             1.f,  1.f,
            -1.f, -1.f,
             1.f,  1.f,
            -1.f,  1.f,
        };
        glGenVertexArrays(1, &m_blitVAO);
        glGenBuffers(1, &m_blitVBO);
        glBindVertexArray(m_blitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_blitVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    glBindVertexArray(m_blitVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ==================== CLEANUP ====================
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::renderSky() 
{
    m_skyTexture.clear(sf::Color::Transparent);
    m_skyTexture.setActive(true); 

    glViewport(0, 0, m_skyTexture.getSize().x, m_skyTexture.getSize().y);
    glUseProgram(m_skyProgram);

    // 1. Send Remaining Celestial Structural Data
    glUniform1i(Uniforms::Sky::UseSkyCubemap, m_skyCubemapReady);
    glUniformMatrix3fv(Uniforms::Sky::StarRotationMatrix, 1, GL_FALSE, &m_astroState.starRotationMatrix[0]);
    
    // 2. Bind Texture Units Deterministically
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCubemapReady ? m_skyCubemapHandle : 0);
    glUniform1i(Uniforms::Sky::Cubemap, 0);

    glActiveTexture(GL_TEXTURE1);
    sf::Texture::bind(&m_moonTexture);
    glUniform1i(Uniforms::Sky::MoonTexture, 1);

    // 3. Procedural Draw Call
    glBindVertexArray(m_gridVAO); 
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Cleanup active bindings
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glUseProgram(0);

    m_skyTexture.setSmooth(true);
    m_skyTexture.display();
}

void Scene_IC_Camp::renderOceanGrid()
{
    // Save previous polygon mode state just to be completely safe
    GLint previousPolygonMode[2];
    glGetIntegerv(GL_POLYGON_MODE, previousPolygonMode);

    // 1. Setup specific forward-rendering conditions
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Switch to manual G-Buffer depth sampling: disable hardware depth discard
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); 

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // 2. Safely activate program
    glUseProgram(m_oceanProgram);
    
    // Calculate the exact world space distance between any two vertices
    float vertexSpacing = m_oceanSize / static_cast<float>(m_oceanResolution);

    // Pull your camera's position out of its transform component
    auto& cameraTransform = m_entityManager.getTransform(m_camera);
    glm::vec3 camPos = toGLMVec3(cameraTransform.pos);

    // Snap the mesh's world translation to perfectly line up with your vertex intervals
    float snappedX = std::floor(camPos.x / vertexSpacing) * vertexSpacing;
    float snappedZ = std::floor(camPos.z / vertexSpacing) * vertexSpacing;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(snappedX, m_seaLevel, snappedZ));
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    
    glUniformMatrix4fv(Uniforms::Ocean::Model, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix3fv(Uniforms::Ocean::NormalMatrix, 1, GL_FALSE, &normalMatrix[0][0]);
    glUniform1f(Uniforms::Ocean::Time, m_game.getElapsedClock().getElapsedTime().asSeconds());

    // =========================================================================
    // Bind Textures for Ocean Pass
    // =========================================================================
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gDepthTex);
    glUniform1i(Uniforms::GBuffer::GDepthTex, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowDepthTexArray);
    glUniform1i(Uniforms::Shadows::ShadowMapArray, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCubemapHandle);
    glUniform1i(Uniforms::Sky::Cubemap, 2);

    glUniform1i(Uniforms::Sky::UseSkyCubemap, m_skyCubemapReady);
    glUniformMatrix3fv(Uniforms::Sky::StarRotationMatrix, 1, GL_FALSE, &m_astroState.starRotationMatrix[0]);
    
    glActiveTexture(GL_TEXTURE3);
    sf::Texture::bind(&m_moonTexture);
    glUniform1i(Uniforms::Sky::MoonTexture, 3);


    // =========================================================================
    // Feed Cascade Uniform Matrices and Configurations
    // =========================================================================
    glUniformMatrix4fv(Uniforms::Shadows::LightViewProj,
                        NUM_CASCADES, GL_FALSE, &m_lightViewProjCascades[0][0][0]);
    glUniform1fv(Uniforms::Shadows::TexelWorldSize, NUM_CASCADES, m_texelWorldSize);
    glUniform1fv(Uniforms::Shadows::CascadeSplitDepths, NUM_CASCADES, m_cascadeSplits);

    glUniform3fv(Uniforms::Ocean::NightAmbientFloor, 1, &m_nightAmbientFloor[0]);

    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);

    // 3. Draw Mesh
    glBindVertexArray(m_oceanVAO);
    glDrawElements(GL_TRIANGLES, m_oceanIndexCount, GL_UNSIGNED_INT, 0);
    
    // ==================== CLEANUP ====================
    glBindVertexArray(0);
    glUseProgram(0);
    if (cullWasEnabled) {
        glEnable(GL_CULL_FACE);
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    sf::Texture::bind(nullptr);
    
    // Reset states back to what SFML/HUD expects
    glPolygonMode(GL_FRONT, previousPolygonMode[0]);
    glPolygonMode(GL_BACK, previousPolygonMode[1]);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

// =========================================================================
// Orb Updates
// =========================================================================

void Scene_IC_Camp::updateOrbBobbing(SoAEntityHandle e, float dt)
{
    if (m_entityManager.getTag(e) != "orb") return;

    auto& t   = m_entityManager.getTransform(e);
    auto& bob = m_entityManager.getBob(e);
    auto& orb = m_entityManager.getOrb(e);

    updateBob(e, dt);

    const float groundY   = heightAt(t.pos.x, t.pos.z);
    const float bobOffset = std::sin(bob.accumulator * 6.2831853f) * bob.magnitude;

    t.pos.y    = groundY + bob.magnitude + bobOffset + orb.radius;
    t.velocity.y = 0.0f;
}

// =========================================================================
// Coordinate and Color Utilities
// =========================================================================

sf::Glsl::Vec3 Scene_IC_Camp::colorToShader(const sf::Color& color) 
{
    return sf::Glsl::Vec3(
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f
    );
}

sf::Vector3f Scene_IC_Camp::screenToWorld(sf::Vector2i position) const 
{
    if (!m_cursorMode) {
        return { 0.f, 0.f, 0.f };
    }

    auto& cam = m_entityManager.getTransform(m_camera);
    auto& camConfig = m_cameraConfig;

    if (position.x < 0 || position.y < 0 ||
        position.x >= (int)camConfig.VIEWPORT_WIDTH || position.y >= (int)camConfig.VIEWPORT_HEIGHT) {
        return { 0.f, 0.f, 0.f };
    }

    float aspectRatio = float(camConfig.VIEWPORT_WIDTH) / camConfig.VIEWPORT_HEIGHT;

    // Reconstruct ray in camera space
    float x_ndc = (position.x / float(camConfig.VIEWPORT_WIDTH)) * 2.0f - 1.0f;
    float y_ndc = 1.0f - (position.y / float(camConfig.VIEWPORT_HEIGHT)) * 2.0f;
    float f = std::tan(camConfig.FOVY * 0.5f);

    sf::Vector3f rayDir = Camera::cameraToWorld(sf::Vector3f(x_ndc * f * aspectRatio, y_ndc * f, -1.0f), cam);
    rayDir = Camera::normalize(rayDir);

    float rayShallowness = std::abs(rayDir.y);
    if (rayShallowness < 0.000001f) {
        rayDir.y = rayDir.y < 0.0f ? -0.000001f : 0.000001f;
        rayShallowness = 0.000001f;
    }

    float t = 0.0f;
    float tPrev = 0.0f;
    bool isHit = false;

    for (int i = 0; i < 512; ++i) {
        if (t > camConfig.FAR_PLANE) {
            break;
        }

        sf::Vector3f p = cam.pos + rayDir * t;
        float h = heightAt(p.x, p.z);
        float distToSurf = p.y - h;

        if (distToSurf < 0.0f) {
            float t_low = tPrev;
            float t_high = t;
            for (int j = 0; j < 12; ++j) {
                float t_mid = 0.5f * (t_low + t_high);
                sf::Vector3f p_mid = cam.pos + rayDir * t_mid;
                if (p_mid.y < heightAt(p_mid.x, p_mid.z)) {
                    t_high = t_mid;
                } else {
                    t_low = t_mid;
                }
            }

            t = 0.5f * (t_low + t_high);
            isHit = true;
            break;
        }

        float angleScale = std::max(rayShallowness, 0.12f);
        float stepSize = std::max(10.0f, (distToSurf / angleScale) * 0.55f);
        stepSize = std::clamp(stepSize, 1.0f, 220.0f);
        tPrev = t;
        t += stepSize;
    }

    if (!isHit) {
        return { 0.f, 0.f, 0.f };
    }

    return {
        cam.pos.x + rayDir.x * t,
        cam.pos.y + rayDir.y * t,
        cam.pos.z + rayDir.z * t
    };
}

float Scene_IC_Camp::getCameraHeightAboveGround(const sf::Vector3f& camPos) const 
{
    float groundHeight = heightAt(camPos.x, camPos.z);
    return camPos.y - groundHeight;
}

void Scene_IC_Camp::sGUI()
{
    ImGui::Begin("Scene Properties##IC_Camp");

    ImGui::Text("FPS: %.1f", m_fps);

    if (ImGui::BeginTabBar("MyTabBar"))
    {
        if (ImGui::BeginTabItem("Debug"))
        {
            ImGui::Checkbox("Draw Grid", &m_drawGrid);
            ImGui::Checkbox("Draw Hex Grid", &m_drawHexGrid);
            ImGui::Checkbox("Draw Textures", &m_drawTextures);
            ImGui::Checkbox("Draw Debug", &m_drawCollision);
            ImGui::Checkbox("Draw Shadows", &m_debugShowShadowMap);
            ImGui::Checkbox("Disable Texel Snap", &m_debugDisableTexelSnap);
            ImGui::Checkbox("Show Cascade Colors", &m_debugShowCascadeColors);
            sf::Vector2i mousePos = m_cachedMousePos;
            sf::Vector2f mouseScreen(float(mousePos.x), float(mousePos.y));

            sf::Vector3f worldPos = screenToWorld(mousePos);
            sf::Vector2i hexCoords = WCH::worldToHex(worldPos.x, worldPos.z);

            auto& playerTransform = m_entityManager.getTransform(m_player);
            sf::Vector3f rel = m_homeLocation3D - playerTransform.pos;
            float dist = std::sqrt(rel.x*rel.x + rel.y*rel.y + rel.z*rel.z);

            ImGui::Text("Mouse screen: (%.1f, %.1f)", mouseScreen.x, mouseScreen.y);
            ImGui::Text("World pos: (%.1f, %.1f, %.1f)", worldPos.x, worldPos.y, worldPos.z);
            ImGui::Text("Hex coords: (%d, %d)", hexCoords.x, hexCoords.y);
            ImGui::Text("Distance: %.1f", dist);
            ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", 
                m_entityManager.getTransform(m_camera).forward().x,
                m_entityManager.getTransform(m_camera).forward().y,
                m_entityManager.getTransform(m_camera).forward().z
            );
            ImGui::Text("Camera Right: (%.2f, %.2f, %.2f)", 
                m_entityManager.getTransform(m_camera).right().x,
                m_entityManager.getTransform(m_camera).right().y,
                m_entityManager.getTransform(m_camera).right().z
            );
            ImGui::Text("Camera Up: (%.2f, %.2f, %.2f)", 
                m_entityManager.getTransform(m_camera).up().x,
                m_entityManager.getTransform(m_camera).up().y,
                m_entityManager.getTransform(m_camera).up().z
            );
            if (ImGui::Button("Reset Orientation", ImVec2(-1.0f, 0.0f))) // -1.0f spans full width of panel
            {
                m_entityManager.getTransform(m_player).setRotation(0.0f, 0.0f, 0.0f);
            }
            ImGui::Separator();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("SSAO Debug"))
        {
            ImGui::Checkbox("Show SSAO Blur", &m_debugShowSSAOBlur);
            ImGui::SliderFloat("SSAO Kernel Radius", &m_debugSSAOKernelRadius, 0.1f, 100.0f);
            ImGui::SliderFloat("SSAO Bias", &m_debugSSAOBias, 0.0001f, 1.0f);
            ImGui::SliderInt("SSAO Sample Count", &m_sampleCount, 1, m_ssaoKernelSize);
            ImGui::Separator();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Wind Tuning"))
        {
            ImGui::SliderFloat("Wind Speed", &m_windSpeed, 0.0f, 10.0f);
            ImGui::Separator();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Entity Manager"))
        {
            auto& m_entities = m_entityManager;
            static std::vector<std::string> tags;
            if (tags.size() != m_entities.getEntityMap().size() + 1)
            {
                tags.clear();
                tags.push_back("ALL");
                for (auto& [tag, entities] : m_entities.getEntityMap())
                {
                    tags.push_back(tag);
                }
            }
            static int currentTagIndex = 0;
            static int currentEntityIndex = 0;

            const char* currentTag = tags[currentTagIndex].c_str();
            if (ImGui::BeginCombo("Tags", currentTag))
            {
                for (int n = 0; n < tags.size(); n++)
                {
                    bool isSelected = (currentTagIndex == n);
                    if (ImGui::Selectable(tags[n].c_str(), isSelected))
                    {
                        currentTagIndex = n;
                        currentEntityIndex = 0;
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            std::vector<SoAEntityHandle> entities;
            if (tags[currentTagIndex] == "ALL") entities = m_entityManager.getEntities();
            else entities = m_entityManager.getEntities(tags[currentTagIndex]);
            if (!entities.empty())
            {
                std::vector<std::string> entityLabels;
                for (auto e : entities)
                {
                    entityLabels.push_back(m_entityManager.getTag(e) + " " + std::to_string(int(e.index)) + " " + std::to_string(int(1)) + "," + std::to_string(int(0)));
                }
                const char* currentEntity = entityLabels[currentEntityIndex].c_str();
                if (ImGui::BeginCombo("Entities", currentEntity))
                {
                    for (int n = 0; n < entityLabels.size(); n++)
                    {
                        bool isSelected = (currentEntityIndex == n);
                        if (ImGui::Selectable(entityLabels[n].c_str(), isSelected))
                            currentEntityIndex = n;
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (!entities.empty())
                {
                    auto entity = entities[currentEntityIndex];
                    ImGui::Text("ID: %d", int(entity.index));
                    ImGui::Text("Tag: %s", m_entityManager.getTag(entity).c_str());
                    ImGui::Button("Destroy Entity");
                    if (ImGui::IsItemClicked())
                    {
                        if (m_entityManager.getTag(entity) != "player")
                        {
                            m_entityManager.destroyEntity(entity);
                            currentEntityIndex = 0;
                        }
                    }
                }
            }
            ImGui::Separator();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Time and Date"))
        {
            bool changed = false;
            float timeOfDayF = static_cast<float>(m_gameTimeOfDay);
            if (ImGui::SliderFloat("Time of Day (hours)", &timeOfDayF, 0.0f, 24.0f))
            {
                m_gameTimeOfDay = static_cast<double>(timeOfDayF);
                changed = true;
            }
            changed |= ImGui::SliderFloat("Latitude", &m_latitude, -90.0f, 90.0f);
            if (changed) {
                updateAstronomySystem();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}