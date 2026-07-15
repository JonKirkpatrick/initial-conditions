#include <GL/glew.h>
#include "Scene_IC_Camp.h"
#include "WorldCoordinates.hpp"
#include "GameEngine.h"
#include "OrbSSBO.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics/CoordinateType.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include "imgui.h"
#include "imgui-SFML.h"
#include "Camera.h"
#include "Astro.hpp"
#include "TerrainStreamer.h"
#include <random>
#include <array>
#include <filesystem>
#include <fstream>

// =========================================================================
// File-Local Static Helper Utilities
// =========================================================================
static sf::Glsl::Mat3 toGlslMat3(const std::array<std::array<float, 3>, 3>& matrix) {
    const float flattened[9] = {
        matrix[0][0], matrix[1][0], matrix[2][0],
        matrix[0][1], matrix[1][1], matrix[2][1],
        matrix[0][2], matrix[1][2], matrix[2][2]
    };

    return sf::Glsl::Mat3(flattened);
}

static glm::vec3 toGLMVec3(const sf::Vector3f& v) {
    return glm::vec3(v.x, v.y, v.z);
}

static glm::vec4 toGLMVec4(const sf::Glsl::Vec4& v) {
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
    m_topdownMaxHeight = 1000.f;
    float worldSize = Topography::BASE_SIZE;
    float worldMinCoord = -worldSize / 2.0f;
    m_topdownWorldMin = { worldMinCoord, worldMinCoord };
    m_topdownWorldSize = { worldSize, worldSize };
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    sf::Vector2u windowSize = game.window().getSize();
    m_moonTexture = Assets::Instance().getTexture("Moon");
    m_skyTexture = sf::RenderTexture({windowSize.x, windowSize.y});
    m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});
    m_topdownTexture = Assets::Instance().getTexture("Test1");
    m_gridColour = Theme::color("cerulean");
    m_cameraConfig.VIEWPORT_WIDTH = windowSize.x;
    m_cameraConfig.VIEWPORT_HEIGHT = windowSize.y;
    initializeGBuffer(m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT);
    loadLevel(m_levelPath);
    spawnPlayer();
    spawnCamera();
    spawnDebugOrbs(32000);
    m_entityManager.update();
    buildTerrainGrid();
    buildHud();
    updateHUDData();
    updateSiderealTime();
    updateSunPosition();
    updateStarRotation();
    updateMoonPosition();
    initializeSkyCubemap();
    initializeShadowMap(m_shadowMapSize);
    initializeOrbShaderStorage();
    m_game.setMouseCaptured(true);
    m_cursorMode = false;
}

Scene_IC_Camp::~Scene_IC_Camp() {
    destroyGBuffer();
    destroyShadowMap();
}

void Scene_IC_Camp::update() {
    m_entityManager.update();
    if (m_cursorMode) {
        m_cachedMousePos = sf::Mouse::getPosition(m_game.window());
        m_leftMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    }
    // Compute delta-time using the game's elapsed clock (wall time)
    float currentTime = m_game.getElapsedClock().getElapsedTime().asSeconds();
    float dt = 1.0f / 60.0f;
    if (m_lastFrameTime > 0.0f) {
        dt = currentTime - m_lastFrameTime;
        // clamp to avoid huge jumps after pauses
        dt = std::clamp(dt, 0.0001f, 0.5f);
    }
    m_lastFrameTime = currentTime;
    // Time advancement — debug rate: 1 in-game hour per 4 real seconds
    // To slow later: replace 1.0f / 4.0f with 1.0f / (4.0f * desiredSlowdown)
    const float realSecondsPerGameHour = 3600.0f;
    m_gameTimeOfDay += static_cast<double>(dt) * (1.0f / realSecondsPerGameHour);
    if (m_gameTimeOfDay >= 24.0f)
    {
        m_gameTimeOfDay -= 24.0f;
        m_gameDayOfMonth++;

        // Days per month, accounting for leap year
        const int daysInMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        bool leapYear = (m_gameYear % 4 == 0 && m_gameYear % 100 != 0) || (m_gameYear % 400 == 0);
        int daysThisMonth = daysInMonth[m_gameMonth] + (leapYear && m_gameMonth == 2 ? 1 : 0);

        if (m_gameDayOfMonth > daysThisMonth)
        {
            m_gameDayOfMonth = 1;
            m_gameMonth++;

            if (m_gameMonth > 12)
            {
                m_gameMonth = 1;
                m_gameYear++;
            }
        }
    }

    sMovement(dt);
    m_entityManager.sUpdateTransformVectors();
    updateCamera(dt);
    updateHUDData();
    updateSiderealTime();
    updateSunPosition();
    updateStarRotation();
    updateMoonPosition();
    updateOrbShaderStorage();

    m_hud->update(m_game.window(), m_hudData);
    if (m_showGUI) {
        sGUI();
    }
    m_fpsFrameCount++;
    float elapsed = m_fpsClock.getElapsedTime().asSeconds();
    if (elapsed >= 0.5f) {
        m_fps = float(m_fpsFrameCount) / elapsed;
        m_fpsFrameCount = 0;
        m_fpsClock.restart();
    }
}

void Scene_IC_Camp::sDoAction(const Action& action) {
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

void Scene_IC_Camp::onEnter() {
    m_entityManager.getInput(m_player).mouseDelta = {0.f, 0.f};
    sf::Vector2u size = m_game.window().getSize();
    sf::Mouse::setPosition(
        sf::Vector2i(size.x / 2, size.y / 2), 
        m_game.window()
    );
    m_game.setMouseCaptured(true);
    m_lastStepPhase = 0.0f;
}

void Scene_IC_Camp::onExit() {
    destroyGBuffer();
    destroyShadowMap();
}

void Scene_IC_Camp::onEnd() {
    destroyGBuffer();
    destroyShadowMap();
}

HUD* Scene_IC_Camp::getHUD() const
{
    return m_hud.get();
}

Topography::TerrainContext Scene_IC_Camp::getTerrainContext() const {
    return Topography::TerrainContext {
        &Assets::Instance().getHeightArray("Test2"),
        m_topdownWorldMin,
        m_topdownWorldSize,
    };
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
                updateSiderealTime();
                updateSunPosition();
                updateStarRotation();
                updateMoonPosition();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Scene_IC_Camp::sRender() {
    auto& window = m_game.window();
    auto& transform = m_entityManager.getTransform(m_camera);
    auto rawWorldToCamMatrix = Camera::getWorldToCamMatrix(transform);
    auto worldToCamMatrix = toGlslMat3(rawWorldToCamMatrix);


    glBindFramebuffer(GL_FRAMEBUFFER, m_gBufferFBO);
    glViewport(0, 0, m_gBufferWidth, m_gBufferHeight);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);

    runTerrainPass(rawWorldToCamMatrix);
    renderOrbCreature();
    renderSky(worldToCamMatrix);
    runShadowPass();

    window.clear(sf::Color::Transparent);
    sf::Sprite backgroundSprite(m_skyTexture.getTexture());
    window.draw(backgroundSprite);
    window.setActive(true);

    if (m_debugShowShadowMap) {
        
    } else {
        deferredLighting();
    }

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
        !m_entityManager.hasPhysics(e) || !m_entityManager.hasBob(e))
        return;

    auto& t     = m_entityManager.getTransform(e);
    auto& input = m_entityManager.getInput(e);
    auto& phys  = m_entityManager.getPhysics(e);
    auto& bob   = m_entityManager.getBob(e);
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

    // === Bobbing & Footsteps ===
    float horizSpeed = std::sqrt(t.velocity.x*t.velocity.x + t.velocity.z*t.velocity.z);
    updateBob(e, dt, horizSpeed);

    if (horizSpeed > 1.0f && phys.onGround)
    {
        auto& bobComp = m_entityManager.getBob(e);
        float currentPhase = bobComp.accumulator;
        bool shouldStep = false;
        bool isLeft = false;

        if ((m_lastStepPhase > 0.8f && currentPhase < 0.2f) || (m_lastStepPhase < 0.2f && currentPhase > 0.8f))
        {
            shouldStep = true; isLeft = true;
        }
        else if ((m_lastStepPhase < 0.45f && currentPhase >= 0.45f) ||
                 (m_lastStepPhase > 0.55f && currentPhase <= 0.55f))
        {
            shouldStep = true; isLeft = false;
        }

        if (shouldStep)
        {
            const std::string& soundName = isLeft ? "FootLeft" : "FootRight";
            float volume = phys.isSprinting ? 75.f : (phys.isCrouching ? 30.f : 45.f);
            AudioManager::Instance().sfx.playSound(Assets::Instance().getSound(soundName), volume);
        }
        m_lastStepPhase = currentPhase;
    }
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

void Scene_IC_Camp::updateSiderealTime()
{
    auto result = Astro::computeSiderealTime(
        m_gameYear,
        m_gameMonth,
        m_gameDayOfMonth,
        m_gameTimeOfDay,
        m_longitude
    );

    m_astroState.astroTime   = result.astroTime;
    m_astroState.epochOffset = result.epochOffset;
}

void Scene_IC_Camp::updateSunPosition()
{
    float declination  = Astro::solarDeclination(m_gameMonth, m_gameDayOfMonth);
    float haDeg        = (m_gameTimeOfDay / 24.0f - 0.5f) * 360.0f;

    auto altaz = Astro::computeAltAz(
        Astro::toRad(haDeg),
        Astro::toRad(declination),
        Astro::toRad(m_latitude)
    );

    m_astroState.sunDirection = Astro::altAzToDirection(altaz.elevationRad, altaz.azimuthRad);

    // === Sun Color & Intensity ===
    float elevationDeg    = Astro::toDeg(altaz.elevationRad);
    float sunHeightFactor = std::clamp((elevationDeg + 12.0f) / 90.0f, 0.0f, 1.0f);
    float warmth          = 1.0f - sunHeightFactor * 0.75f;

    m_astroState.sunColor = sf::Glsl::Vec4(
        1.00f,
        0.90f + warmth * 0.10f,
        0.65f + warmth * 0.30f,
        sunHeightFactor * 1.25f + 0.25f
    );
}

void Scene_IC_Camp::updateStarRotation()
{
    Astro::computeStarRotationMatrix(
        m_latitude,
        m_longitude,
        m_astroState.epochOffset,
        m_astroState.starRotationMatrix
    );
}

void Scene_IC_Camp::updateMoonPosition()
{
    m_astroState.moonDirection = Astro::computeMoonDirection(
        m_astroState.astroTime,
        m_latitude
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
    m_entityManager.addBob(m_player, CBob(1.0f, 0.06f, 0.055f));   // rate, vertical mag, lateral mag
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
    // 1. Calculate the spatial data right now (no change here)
    const sf::Vector2f groundXZ = WCH::hexToWorld(hexQ, hexR);
    const float        groundY  = heightAt(groundXZ.x, groundXZ.y);
    const float        hoverY   = groundY + radius;

    CTransform3D t(sf::Vector3f(groundXZ.x, hoverY, groundXZ.y));
    
    // =========================================================================
    // UPGRADE: Convert the initial yaw from radians to your quaternion orientation.
    // (Pitch = 0.0f, Yaw = converted to degrees, Roll = 0.0f)
    // =========================================================================
    float yawDeg = yawRad * (180.f / 3.14159265f);
    t.setRotation(0.0f, yawDeg, 0.0f);

    // 2. Queue the spawn and pass a lambda to handle the lazy initialization
    m_entityManager.queueSpawn("orb", [=](SoAEntityHandle orb, EntityManager& em) 
    {
        // This code executes safely during the main thread sync phase!
        em.addTransform(orb, t);
        em.addOrb(orb, COrb(radius));
        em.addBob(orb, CBob(bobRate, bobMagnitude, 0.0f));
        em.addEyes(orb, eyes);
        em.getOrb(orb).speciesIdx = species;
    });
}

void Scene_IC_Camp::spawnDebugOrbs(int count)
{
    // 1. Random Number Engine Setup
    std::mt19937 rng(1337); // Seeded for consistency
    
    // Extents are -100 to 100 tiles. 
    std::uniform_int_distribution<int> hexDist(-1000, 1000);
    
    std::uniform_real_distribution<float> radiusDist(0.2f, 1.5f);
    std::uniform_real_distribution<float> bobRateDist(0.2f, 1.0f);
    std::uniform_real_distribution<float> bobMagDist(0.05f, 0.5f);
    std::uniform_real_distribution<float> gazeDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> dilationDist(0.5f, 1.0f);
    std::uniform_real_distribution<float> closureDist(0.0f, 0.0f);
    std::uniform_real_distribution<float> yawDist(0.0f, 2.0f * 3.141592f); // Generates in Radians
    std::uniform_int_distribution<int> speciesDist(0, 6);

    // 2. Spatial Coalescing & Hashing
    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 16);
        }
    };
    std::unordered_set<std::pair<int,int>, PairHash> usedCoords;

    const int minHexDistance = 3; 

    auto IsTooClose = [&](int q, int r) {
        for (const auto& [uq, ur] : usedCoords) {
            int dist = (std::abs(q - uq) + std::abs(q + r - uq - ur) + std::abs(r - ur)) / 2;
            if (dist < minHexDistance) {
                return true;
            }
        }
        return false;
    };

    int spawned     = 0;
    int maxAttempts = count * 50; 
    int attempts    = 0;

    while (spawned < count && attempts < maxAttempts)
    {
        ++attempts;
        int hexQ = hexDist(rng);
        int hexR = hexDist(rng);

        if (IsTooClose(hexQ, hexR)) continue;

        usedCoords.insert({hexQ, hexR});

        float radius           = radiusDist(rng);
        float bobRate          = bobRateDist(rng);
        float bobMag           = bobMagDist(rng);
        float yaw              = yawDist(rng); // Radians match perfectly
        int species            = speciesDist(rng);

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

        // Passed safely over to be handled inside spawnOrbFauna
        spawnOrbFauna(hexQ, hexR, radius, bobRate, bobMag, eyes, yaw, species);
        ++spawned;
    }
}

void Scene_IC_Camp::buildTerrainGrid() {
    const int W = Topography::GRID_RESOLUTION;
    const int H = Topography::GRID_RESOLUTION;

    std::vector<float> verts;
    verts.reserve(W * H * 2);

    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            // Generate a normalized coordinate mapping from 0.0 to 1.0
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

// =========================================================================
// Rendering Systems & Pipeline
// =========================================================================

void Scene_IC_Camp::updateCamera(float dt) 
{
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& playerTransform = m_entityManager.getTransform(m_player);
    auto& playerPhysics = m_entityManager.getPhysics(m_player);
    auto& playerBob = m_entityManager.getBob(m_player);
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
    float phase = playerBob.accumulator * 6.2831853f;

    // === Bob Parameters ===
    float baseFrequency = 1.0f;

    float t = std::clamp((speedFraction - 1.0f) / 2.0f, 0.0f, 1.0f); // 0 = walk, 1 = full sprint
    float lateralAmplitude = 0.055f + (0.038f - 0.055f) * t;
    float verticalAmplitude = 0.062f + (0.048f - 0.062f) * t;

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

void Scene_IC_Camp::initializeSkyCubemap()
{

    m_skyCubemapHandle = Assets::Instance().getCubemap("NightSky");
    m_skyCubemapReady  = (m_skyCubemapHandle != 0);

    if (!m_skyCubemapReady)
        std::cerr << "Sky cubemap 'NightSky' not found in Assets; falling back to procedural sky." << std::endl;
}

void Scene_IC_Camp::initializeShadowMap(unsigned int size) {
    m_shadowMapSize = size;

    glGenTextures(1, &m_shadowDepthTexArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowDepthTexArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 size, size, NUM_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    // --- 1. CHANGE TO LINEAR FILTERS ---
    // Hardware PCF requires linear filtering parameters so the GPU knows it 
    // is allowed to interpolate between neighboring depth comparison results.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // --- 2. ENABLE SHADOW COMPARISON MODE ---
    // This tells the texture unit to treat this texture as a shadow map, changing
    // its behavior from returning raw depth to returning a PCF-blended visibility factor.
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

void Scene_IC_Camp::destroyShadowMap() {
    if (m_shadowFBO) {
        glDeleteFramebuffers(1, &m_shadowFBO);
        glDeleteTextures(1, &m_shadowDepthTexArray);
        m_shadowFBO = 0;
        m_shadowDepthTexArray = 0;
    }
}

float Scene_IC_Camp::computeDistanceToMapFarCorner(const glm::vec3& camPos) const {
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

void Scene_IC_Camp::computeCascadeSplits(float camNear, float camFar) {
    for (int i = 0; i < NUM_CASCADES; ++i) {
        float p = float(i + 1) / float(NUM_CASCADES);
        float logSplit     = camNear * std::pow(camFar / camNear, p);
        float uniformSplit = camNear + (camFar - camNear) * p;
        m_cascadeSplits[i] = m_cascadeSplitLambda * logSplit
                           + (1.0f - m_cascadeSplitLambda) * uniformSplit;
    }

    // Override the last split: instead of following the log/uniform scheme,
    // extend it all the way to the farthest corner of the currently loaded map.
    auto& camTransform = m_entityManager.getTransform(m_camera);
    float mapEdgeDist  = computeDistanceToMapFarCorner(toGLMVec3(camTransform.pos));

    // Guard against the map-edge distance ever being smaller than the
    // previous split (e.g. camera standing right at the map boundary) --
    // runShadowPass depends on splits being strictly increasing.
    m_cascadeSplits[NUM_CASCADES - 1] =
        std::max(mapEdgeDist, m_cascadeSplits[NUM_CASCADES - 2] + 1.0f);
}

glm::mat4 Scene_IC_Camp::computeLightViewProjForRange(float splitNear, float splitFar, float& lightDepthRange, float& texelWorldSize) const {
    glm::vec3 sunDir = glm::normalize(toGLMVec3(m_astroState.sunDirection));
    glm::vec3 worldUp = (std::abs(sunDir.y) > 0.99f) ? glm::vec3(0.f, 0.f, 1.f)
                                                      : glm::vec3(0.f, 1.f, 0.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -sunDir, worldUp);

    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData      = m_entityManager.getCamera(m_camera);

    // Build a sub-frustum covering only [splitNear, splitFar] of the camera's view.
    CCamera subFrustumCamData = camData;
    subFrustumCamData.nearPlane = splitNear;
    subFrustumCamData.farPlane  = splitFar;

    auto vp = Camera::getVPMatrix(camTransform, subFrustumCamData);
    glm::mat4 camVP    = glm::make_mat4(vp.data());
    glm::mat4 camInvVP = glm::inverse(camVP);

    static const glm::vec3 ndcCorners[8] = {
        {-1,-1, 0}, { 1,-1, 0}, { 1, 1, 0}, {-1, 1, 0},
        {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
    };

    glm::vec3 worldCorners[8];
    glm::vec3 centroid(0.0f);
    for (int i = 0; i < 8; ++i) {
        glm::vec4 p = camInvVP * glm::vec4(ndcCorners[i], 1.0f);
        worldCorners[i] = glm::vec3(p) / p.w;
        centroid += worldCorners[i];
    }
    centroid /= 8.0f;

    float radius = 0.0f;
    for (int i = 0; i < 8; ++i)
        radius = std::max(radius, glm::length(worldCorners[i] - centroid));
    radius += m_shadowFrustumPadding;

    float worldUnitsPerTexel = (radius * 2.0f) / float(m_shadowMapSize);
    texelWorldSize = worldUnitsPerTexel; // NEW: stash this before nearExtension distorts anything

    glm::vec3 centroidLS = glm::vec3(lightView * glm::vec4(centroid, 1.0f));
    centroidLS.x = std::floor(centroidLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    centroidLS.y = std::floor(centroidLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    float nearExtension = m_shadowFrustumPadding * 20.0f;
    centroidLS.z += radius + nearExtension;

    glm::mat4 lightViewInv = glm::inverse(lightView);
    glm::vec3 eyeWorldPos  = glm::vec3(lightViewInv * glm::vec4(centroidLS, 1.0f));
    glm::mat4 finalLightView = glm::lookAt(eyeWorldPos, eyeWorldPos - sunDir, worldUp);

    float farDist = radius * 2.0f + nearExtension;
    lightDepthRange = farDist;
    return glm::ortho(-radius, radius, -radius, radius, 0.0f, farDist) * finalLightView;
}

glm::mat4 Scene_IC_Camp::computeLightViewProjForMapBounds(float& lightDepthRange, float& texelWorldSize) const {
    glm::vec3 sunDir = glm::normalize(toGLMVec3(m_astroState.sunDirection));
    glm::vec3 worldUp = (std::abs(sunDir.y) > 0.99f) ? glm::vec3(0.f, 0.f, 1.f)
                                                      : glm::vec3(0.f, 1.f, 0.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -sunDir, worldUp);

    glm::vec2 mapMin(m_topdownWorldMin.x, m_topdownWorldMin.y);
    glm::vec2 mapMax(mapMin.x + m_topdownWorldSize.x, mapMin.y + m_topdownWorldSize.y);
    float heightMin = 0.0f; // adjust if your terrain can go negative
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
    for (auto& c : worldCorners)
        radius = std::max(radius, glm::length(c - centroid));
    radius += m_shadowFrustumPadding;

    float worldUnitsPerTexel = (radius * 2.0f) / float(m_shadowMapSize);
    texelWorldSize = worldUnitsPerTexel;

    glm::vec3 centroidLS = glm::vec3(lightView * glm::vec4(centroid, 1.0f));
    centroidLS.x = std::floor(centroidLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    centroidLS.y = std::floor(centroidLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    float nearExtension = m_shadowFrustumPadding * 20.0f;
    centroidLS.z += radius + nearExtension;

    glm::mat4 lightViewInv = glm::inverse(lightView);
    glm::vec3 eyeWorldPos  = glm::vec3(lightViewInv * glm::vec4(centroidLS, 1.0f));
    glm::mat4 finalLightView = glm::lookAt(eyeWorldPos, eyeWorldPos - sunDir, worldUp);

    float farDist = radius * 2.0f + nearExtension;
    lightDepthRange = farDist;
    return glm::ortho(-radius, radius, -radius, radius, 0.0f, farDist) * finalLightView;
}

void Scene_IC_Camp::runShadowPass() {
    auto& camData = m_entityManager.getCamera(m_camera);
    float clampedFar = std::min(camData.farPlane, m_shadowMaxDistance);
    computeCascadeSplits(camData.nearPlane, clampedFar);

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glViewport(0, 0, m_shadowMapSize, m_shadowMapSize);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    float splitNear = camData.nearPlane;

    for (int cascade = 0; cascade < NUM_CASCADES; ++cascade) {
        float splitFar = m_cascadeSplits[cascade];

        if (cascade == NUM_CASCADES - 1) {
            // Fill cascade: fit to the loaded map's world bounds instead of
            // the camera sub-frustum for this range.
            m_lightViewProjCascades[cascade] = computeLightViewProjForMapBounds(
                m_lightDepthRange[cascade], m_texelWorldSize[cascade]);
        } else {
            m_lightViewProjCascades[cascade] = computeLightViewProjForRange(
                splitNear, splitFar, m_lightDepthRange[cascade], m_texelWorldSize[cascade]);
        }
        splitNear = splitFar; // next cascade picks up where this one left off

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   m_shadowDepthTexArray, 0, cascade);
        glClear(GL_DEPTH_BUFFER_BIT);

        // --- terrain ---
        glDisable(GL_CULL_FACE);
        glUseProgram(m_terrainShadowProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_topdownTexture.getNativeHandle());
        glUniform1i(glGetUniformLocation(m_terrainShadowProgram, "u_topoTopdownTex"), 0);
        glUniform2f(glGetUniformLocation(m_terrainShadowProgram, "u_topdownWorldMin"),
                    m_topdownWorldMin.x, m_topdownWorldMin.y);
        glUniform2f(glGetUniformLocation(m_terrainShadowProgram, "u_topdownWorldSize"),
                    m_topdownWorldSize.x, m_topdownWorldSize.y);
        glUniform1f(glGetUniformLocation(m_terrainShadowProgram, "u_heightMax"), m_topdownMaxHeight);
        glUniformMatrix4fv(glGetUniformLocation(m_terrainShadowProgram, "u_lightViewProj"),
                           1, GL_FALSE, &m_lightViewProjCascades[cascade][0][0]);
        glBindVertexArray(m_gridVAO);
        glDrawElements(GL_TRIANGLES, m_gridIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // --- orbs ---
        glDisable(GL_CULL_FACE);
        glUseProgram(m_orbShadowProgram);
        glm::vec3 sunDir = toGLMVec3(m_astroState.sunDirection);
        glUniformMatrix4fv(glGetUniformLocation(m_orbShadowProgram, "u_lightViewProj"),
                           1, GL_FALSE, &m_lightViewProjCascades[cascade][0][0]);
        glUniform3fv(glGetUniformLocation(m_orbShadowProgram, "u_lightDir"),
                    1, glm::value_ptr(sunDir));
        m_orbSSBO.bind(0);
        glBindVertexArray(m_cubeVAO);
        glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, m_orbSSBO.count());
        glBindVertexArray(0);
    }

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::initializeGBuffer(unsigned int width, unsigned int height) {
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

void Scene_IC_Camp::destroyGBuffer() {
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
        auto f = Camera::getForward(t);
        auto r = Camera::getRight(t);
        auto u = Camera::getUp(t);

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
    m_orbSSBO.upload(buildOrbData());
}

void Scene_IC_Camp::updateOrbShaderStorage()
{
    auto orbData = buildOrbData();
    if (m_orbSSBO.count() != static_cast<int>(orbData.size()))
        m_orbSSBO.upload(orbData);
    else
        m_orbSSBO.update(orbData);
}

void Scene_IC_Camp::runTerrainPass(const std::array<std::array<float, 3>, 3>& worldToCamMatrix) {
    auto& transform  = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    auto vpMatrix   = Camera::getVPMatrix(transform, cameraData);
    sf::Vector3f worldPos = screenToWorld(m_cachedMousePos);
    sf::Vector2i hex = WCH::worldToHex(worldPos.x, worldPos.z);

    sf::Vector2u windowSize = m_game.window().getSize();

    glUseProgram(m_terrainProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_topdownTexture.getNativeHandle());

    // Uniforms for the Vertex Portion of the Shader
    glUniform1i(glGetUniformLocation(m_terrainProgram, "u_topoTopdownTex"), 0);
    glUniform2f(glGetUniformLocation(m_terrainProgram, "u_topdownWorldMin"),
                m_topdownWorldMin.x, m_topdownWorldMin.y);
    glUniform2f(glGetUniformLocation(m_terrainProgram, "u_topdownWorldSize"),
                m_topdownWorldSize.x, m_topdownWorldSize.y);
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_heightMax"), m_topdownMaxHeight);
    glUniformMatrix4fv(glGetUniformLocation(m_terrainProgram, "u_viewProj"),
                       1, GL_FALSE, vpMatrix.data());

    // Uniforms for the Fragment Portion of the Shader
    glUniform3f(glGetUniformLocation(m_terrainProgram, "u_cameraPos"), transform.pos.x, transform.pos.y, transform.pos.z);
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_cameraHeight"), getCameraHeightAboveGround(transform.pos));
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_farPlane"), cameraData.farPlane);
    glUniformMatrix3fv(glGetUniformLocation(m_terrainProgram, "u_worldToCamMatrix"), 1, GL_FALSE, &worldToCamMatrix[0][0]);

    glUniform3f(glGetUniformLocation(m_terrainProgram, "u_sunDir"), m_astroState.sunDirection.x, m_astroState.sunDirection.y, m_astroState.sunDirection.z);
    glUniform4f(glGetUniformLocation(m_terrainProgram, "u_sunColor"), m_astroState.sunColor.x, m_astroState.sunColor.y, m_astroState.sunColor.z, m_astroState.sunColor.w);
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_ambientStrength"), m_sunIntensity);

    glUniform1i(glGetUniformLocation(m_terrainProgram, "u_headlampOn"), shouldHeadlightsBeOn() ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_headlampIntensity"), 4.0f);
    glUniform3f(glGetUniformLocation(m_terrainProgram, "u_headlampColour"), 255.f / 255.f, 244.f / 255.f, 214.f / 255.f);
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_headlampRange"), 200.0f);
    
    glUniform1i(glGetUniformLocation(m_terrainProgram, "u_cursorMode"), static_cast<int>(m_cursorMode));
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_hexSize"), m_hexSize);
    glUniform2f(glGetUniformLocation(m_terrainProgram, "u_hoveredHex"), hex.x, hex.y);
    glUniform3f(glGetUniformLocation(m_terrainProgram, "u_gridColour"), m_gridColour.r / 255.f, m_gridColour.g / 255.f, m_gridColour.b / 255.f);
    
    glUniform1f(glGetUniformLocation(m_terrainProgram, "u_reliefExaggeration"), 1.0f);

    glBindVertexArray(m_gridVAO);
    glDrawElements(GL_TRIANGLES, m_gridIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
    sf::Shader::bind(nullptr);
    sf::Texture::bind(nullptr);
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

    // == Draw the hillshaded topo layer via shader ==========================
    m_minimapTexture.clear(sf::Color::Transparent);

    sf::RectangleShape fullQuad({texSize, texSize});

    m_topoMinimapShader.setUniform("topdownWorldMin", sf::Glsl::Vec2(m_topdownWorldMin.x, m_topdownWorldMin.y));
    m_topoMinimapShader.setUniform("topdownWorldSize", sf::Glsl::Vec2(m_topdownWorldSize.x, m_topdownWorldSize.y));
    m_topoMinimapShader.setUniform("topoTopdownTex", m_topdownTexture);
    m_topoMinimapShader.setUniform("u_playerXZ",
        sf::Glsl::Vec2(playerPos.x, playerPos.z));
    m_topoMinimapShader.setUniform("u_worldRadius",  worldRadius);
    m_topoMinimapShader.setUniform("u_texSize",      texSize);
    m_topoMinimapShader.setUniform("u_heightMax",    m_topdownMaxHeight);
    sf::RenderStates states;
    states.shader = &m_topoMinimapShader;
    m_minimapTexture.draw(fullQuad, states);

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
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData      = m_entityManager.getCamera(m_camera);

    glm::vec3 camPos   = toGLMVec3(camTransform.pos);
    glm::vec3 camFwd   = toGLMVec3(Camera::getForward(camTransform));
    glm::vec3 camRight = toGLMVec3(Camera::getRight(camTransform));
    glm::vec3 camUp    = toGLMVec3(Camera::getUp(camTransform));
    glm::vec3 sunDir   = toGLMVec3(m_astroState.sunDirection);
    glm::vec4 sunColor = toGLMVec4(m_astroState.sunColor);
    auto      vp       = Camera::getVPMatrix(camTransform, camData);

    // ==================== LAZY INIT ====================
    if (m_cubeVAO == 0)
        buildVertexCube();

    // ==================== PROGRAM ====================
    glUseProgram(m_OrbCreatureProgram);

    // ==================== TEXTURES ====================
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Assets::Instance().getSpeciesDiffuseArray());
    glUniform1i(glGetUniformLocation(m_OrbCreatureProgram, "u_charDiffuseTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Assets::Instance().getSpeciesNormalArray());
    glUniform1i(glGetUniformLocation(m_OrbCreatureProgram, "u_charNormalTex"), 1);

    // ==================== UNIFORMS ====================
    glUniform2f(glGetUniformLocation(m_OrbCreatureProgram, "u_viewportSize"),
        static_cast<float>(camData.viewportSize.x),
        static_cast<float>(camData.viewportSize.y));
    glUniform1f(glGetUniformLocation(m_OrbCreatureProgram,  "u_fovY"),          camData.fovY);
    glUniform3fv(glGetUniformLocation(m_OrbCreatureProgram, "u_cameraPos"),     1, glm::value_ptr(camPos));
    glUniform3fv(glGetUniformLocation(m_OrbCreatureProgram, "u_cameraForward"), 1, glm::value_ptr(camFwd));
    glUniform3fv(glGetUniformLocation(m_OrbCreatureProgram, "u_cameraRight"),   1, glm::value_ptr(camRight));
    glUniform3fv(glGetUniformLocation(m_OrbCreatureProgram, "u_cameraUp"),      1, glm::value_ptr(camUp));
    glUniformMatrix4fv(glGetUniformLocation(m_OrbCreatureProgram, "u_viewProj"),
        1, GL_FALSE, vp.data());

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
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_gAlbedo"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_gNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_gIndicesTex);
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_gIndices"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_gRetroTex);
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_gRetro"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_gDepthTex);
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_gDepth"), 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowDepthTexArray);
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_shadowMap"), 5);
    glUniformMatrix4fv(glGetUniformLocation(m_lightingProgram, "u_lightViewProj"),
                        NUM_CASCADES, GL_FALSE, &m_lightViewProjCascades[0][0][0]);
    glUniform1fv(glGetUniformLocation(m_lightingProgram, "u_lightDepthRange"), NUM_CASCADES, m_lightDepthRange);
    glUniform1fv(glGetUniformLocation(m_lightingProgram, "u_texelWorldSize"), NUM_CASCADES, m_texelWorldSize);
    glUniform1fv(glGetUniformLocation(m_lightingProgram, "u_cascadeSplitDepths"), NUM_CASCADES, m_cascadeSplits);
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_debugShowCascadeColors"), m_debugShowCascadeColors ? 1 : 0);

    // =========================================================================
    // Bind SSBOs
    // =========================================================================
    Assets::Instance().getSpeciesSSBO().bind(1);
    Assets::Instance().getMaterialSSBO().bind(2);

    // =========================================================================
    // Fetch Camera, Spatial, and Astro Math Vectors
    // =========================================================================
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData      = m_entityManager.getCamera(m_camera);

    glm::vec3 camPos   = toGLMVec3(camTransform.pos);
    glm::vec3 camFwd   = toGLMVec3(Camera::getForward(camTransform));
    glm::vec3 sunDir   = toGLMVec3(m_astroState.sunDirection);
    glm::vec4 sunColor = toGLMVec4(m_astroState.sunColor);

    auto vp = Camera::getVPMatrix(camTransform, camData);

    glm::mat4 glmVP = glm::make_mat4(vp.data());

    glm::mat4 invVP = glm::inverse(glmVP);

    // =========================================================================
    // Forward Uniform State to Deferred Lighting Program
    // =========================================================================
    glUniform3fv(glGetUniformLocation(m_lightingProgram, "u_cameraPos"),     1, &camPos[0]);
    glUniform3fv(glGetUniformLocation(m_lightingProgram, "u_cameraForward"), 1, &camFwd[0]);
    glUniform3fv(glGetUniformLocation(m_lightingProgram, "u_sunDir"),        1, &sunDir[0]);
    glUniform4fv(glGetUniformLocation(m_lightingProgram, "u_sunColor"),      1, &sunColor[0]);
    
    glUniformMatrix4fv(glGetUniformLocation(m_lightingProgram, "u_invViewProj"), 1, GL_FALSE, &invVP[0][0]);

    // Headlamp Configuration (matches your orb configuration schema)
    glUniform1f(glGetUniformLocation(m_lightingProgram, "u_headlampIntensity"), 2.0f);
    glUniform1f(glGetUniformLocation(m_lightingProgram, "u_headlampRange"),     200.0f);
    glUniform1f(glGetUniformLocation(m_lightingProgram, "u_headlampConeCos"),   1.0f);
    glUniform1f(glGetUniformLocation(m_lightingProgram, "u_headlampEnabled"),   shouldHeadlightsBeOn() ? 1.0f : 0.0f);

    // =========================================================================
    // Bind Heightmap and Topography Textures
    // =========================================================================
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_topdownTexture.getNativeHandle());
    glUniform1i(glGetUniformLocation(m_lightingProgram, "u_topoTopdownTex"), 6);
    glUniform2f(glGetUniformLocation(m_lightingProgram, "u_topdownWorldMin"),
                m_topdownWorldMin.x, m_topdownWorldMin.y);
    glUniform2f(glGetUniformLocation(m_lightingProgram, "u_topdownWorldSize"),
                m_topdownWorldSize.x, m_topdownWorldSize.y);
    glUniform1f(glGetUniformLocation(m_lightingProgram, "u_heightMax"), m_topdownMaxHeight);


    // =========================================================================
    // Draw
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
    glUniform1i(glGetUniformLocation(m_blitProgram, "u_tex"), 0);

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

void Scene_IC_Camp::renderSky(const sf::Glsl::Mat3& worldToCamMatrix) {
    auto& cameraData = m_entityManager.getCamera(m_camera);
    auto transform = m_entityManager.getTransform(m_camera);
    sf::RectangleShape skyQuad(sf::Vector2f(m_game.window().getSize().x, m_game.window().getSize().y));
    m_sky.setUniform("viewportSize",  sf::Glsl::Vec2(m_game.window().getSize().x, m_game.window().getSize().y));
    m_sky.setUniform("fovY",          cameraData.fovY);
    m_sky.setUniform("aspectRatio",   cameraData.aspectRatio);
    m_sky.setUniform("worldToCamMatrix", worldToCamMatrix);
    m_sky.setUniform("sunDir", m_astroState.sunDirection);
    m_sky.setUniform("useSkyCubemap", m_skyCubemapReady);
    m_sky.setUniform("starRotationMatrix", sf::Glsl::Mat3(m_astroState.starRotationMatrix));
    m_sky.setUniform("skyExposure", 5.0f);
    m_sky.setUniform("moonDir", m_astroState.moonDirection);
    m_sky.setUniform("moonTexture", m_moonTexture);

    if (m_skyCubemapReady && m_skyCubemapHandle != 0)
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCubemapHandle);
    }
    m_skyTexture.clear(sf::Color::Transparent);
    m_skyTexture.draw(skyQuad, &m_sky);
    m_skyTexture.setSmooth(true);
    m_skyTexture.display();
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

    // World convention: Y is vertical. XZ is the ground plane.
    // Orb hovers at bobMagnitude above ground, oscillating downward from there.
    const float groundY   = heightAt(t.pos.x, t.pos.z);
    const float bobOffset = std::sin(bob.accumulator * 6.2831853f) * bob.magnitude;

    t.pos.y    = groundY + bob.magnitude + bobOffset + orb.radius;
    t.velocity.y = 0.0f;
}

// =========================================================================
// Coordinate and Color Utilities
// =========================================================================

sf::Glsl::Vec3 Scene_IC_Camp::colorToShader(const sf::Color& color) {
    return sf::Glsl::Vec3(
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f
    );
}

sf::Vector3f Scene_IC_Camp::screenToWorld(sf::Vector2i position) const {
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

float Scene_IC_Camp::getCameraHeightAboveGround(const sf::Vector3f& camPos) const {
    float groundHeight = heightAt(camPos.x, camPos.z);
    return camPos.y - groundHeight;
}