#include <GL/glew.h>
#include "Scene_IC_Camp.h"
#include "GameEngine.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics/CoordinateType.hpp>
#include <cmath>
#include "imgui.h"
#include "imgui-SFML.h"
#include "Camera.h"
#include "Astro.hpp"
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

static sf::Vector3f forwardFromTransform(const CTransform3D& transform)
{
    return Camera::cameraToWorld(sf::Vector3f(0.f, 0.f, -1.f), transform.pitch, transform.yaw, transform.roll);
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
// Public Interface
// =========================================================================

Scene_IC_Camp::Scene_IC_Camp(GameEngine& game, const std::string& levelPath)
    : Scene(game)
    , m_levelPath(levelPath)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    sf::Vector2u windowSize = game.window().getSize();
    m_moonTexture = Assets::Instance().getTexture("Moon");
    m_renderTexture = sf::RenderTexture({windowSize.x, windowSize.y});
    m_skyTexture = sf::RenderTexture({windowSize.x, windowSize.y});
    m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});
    m_bakeTexture = sf::RenderTexture({windowSize.x, windowSize.y}, sf::ContextSettings(24));
    m_bakeTexture.setSmooth(false);
    m_topdownTexture = Assets::Instance().getTexture("Test1");
    m_topdownImage = m_topdownTexture.copyToImage();
    float worldSize = Topography::BASE_SIZE;
    float worldMinCoord = -worldSize / 2.0f;
    m_topdownWorldMin = { worldMinCoord, worldMinCoord };
    m_topdownWorldSize = { worldSize, worldSize };
    m_gridColor = Theme::color("cerulean");
    GLint linked;
    glGetProgramiv(m_bakeProgram, GL_LINK_STATUS, &linked);
    m_cameraConfig.VIEWPORT_WIDTH = windowSize.x;
    m_cameraConfig.VIEWPORT_HEIGHT = windowSize.y;
    loadLevel(m_levelPath);
    spawnPlayer();
    spawnCamera();
    updateCamera(0.001f);
    spawnDebugOrbs(8000);

    m_entityManager.update();

    m_topdownMaxHeight = 100000.f;
    buildTerrainGrid();
    buildHud();
    updateHUDData();
    updateSiderealTime();
    updateSunPosition();
    updateStarRotation();
    updateMoonPosition();
    initializeSkyCubemap();
    initializeBakeFBO();
    m_game.setMouseCaptured(true);
    m_cursorMode = false;
}

void Scene_IC_Camp::update() {
    m_entityManager.update();
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
    const float realSecondsPerGameHour = 30.0f;
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
    updateCamera(dt);
    updateHUDData();
    updateSiderealTime();
    updateSunPosition();
    updateStarRotation();
    updateMoonPosition();
    sortOrbs();
    m_hud->update(m_game.window(), m_hudData);
    if (m_showGUI) {
        sGUI();
    }
    // FPS sampling: update once every 0.5s for a stable reading
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
    // TODO: Called when exiting the scene
}

void Scene_IC_Camp::onEnd() {
    // TODO: Called when ending the scene
}

HUD* Scene_IC_Camp::getHUD() const
{
    return m_hud.get();
}

Topography::TerrainContext Scene_IC_Camp::getTerrainContext() const {
    return Topography::TerrainContext{
        m_topdownImage,
        m_topdownWorldMin,
        m_topdownWorldSize,
        m_topdownMaxHeight
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
            sf::Vector2i mousePos = sf::Mouse::getPosition(m_game.window());
            sf::Vector2f mouseScreen(float(mousePos.x), float(mousePos.y));

            sf::Vector3f worldPos = screenToWorld(mousePos);
            sf::Vector2i hexCoords = worldToHex(worldPos.x, worldPos.z);

            auto& playerTransform = m_entityManager.getTransform(m_player);
            sf::Vector3f rel = worldPos - playerTransform.pos;
            float dist = std::sqrt(rel.x*rel.x + rel.y*rel.y + rel.z*rel.z);

            ImGui::Text("Mouse screen: (%.1f, %.1f)", mouseScreen.x, mouseScreen.y);
            ImGui::Text("World pos: (%.1f, %.1f, %.1f)", worldPos.x, worldPos.y, worldPos.z);
            ImGui::Text("Hex coords: (%d, %d)", hexCoords.x, hexCoords.y);
            ImGui::Text("Distance: %.1f", dist);
            ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", 
                forwardFromTransform(m_entityManager.getTransform(m_camera)).x, 
                forwardFromTransform(m_entityManager.getTransform(m_camera)).y, 
                forwardFromTransform(m_entityManager.getTransform(m_camera)).z);

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
    auto worldToCamMatrix = toGlslMat3(Camera::getWorldToCamMatrix(transform.pitch, transform.yaw, transform.roll));
    window.clear(sf::Color::Transparent);
    updateShadowOrbs();
    uploadShadowOrbsToShader(m_terrainShader);
    runBakePass();
    // debugDumpBakeTexture();
    renderSky(worldToCamMatrix);
    runTerrainPass(worldToCamMatrix);
    sf::Sprite finalSprite(m_renderTexture.getTexture());
    sf::Sprite backgroundSprite(m_skyTexture.getTexture());
    window.draw(backgroundSprite);
    window.draw(finalSprite);
    renderOrbs();
    renderDemoSphere();
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

    // === Terrain Info ===
    sf::Vector3f flatForward = forwardFromTransform(t);
    flatForward.y = 0.0f;
    flatForward = Camera::normalize(flatForward);

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
            // Very gentle momentum boost
            slopeFactor = 1.0f + std::clamp(grade * 0.65f, 0.0f, 0.22f);
        }
        else // === UPHILL ===
        {
            // Very gentle retardation
            slopeFactor = 1.0f - std::clamp(grade * 1.1f, 0.0f, 0.18f);
        }
    }

    moveSpeed *= slopeFactor;
    // === Rotation ===
    t.yaw += input.mouseDelta.x * 0.002f;
    t.pitch += input.mouseDelta.y * 0.002f;
    input.mouseDelta = {0.f, 0.f};

    if (input.strafe)
    {
        if (input.left) t.yaw -= m_playerConfig.ROTATION_SPEED * dt;
        if (input.right) t.yaw += m_playerConfig.ROTATION_SPEED * dt;
    }
    t.pitch = std::clamp(t.pitch, -1.57f, 1.57f);

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
            // === Velocity follows the slope naturally ===
            sf::Vector3f desiredOnPlane = desired - normal * dot(desired, normal);
            if (length(desiredOnPlane) > 0.001f)
                desiredOnPlane = Camera::normalize(desiredOnPlane) * moveSpeed;

            t.velocity.x = desiredOnPlane.x;
            t.velocity.z = desiredOnPlane.z;
            t.velocity.y = desiredOnPlane.y;
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

        if (heightDiff < -20.0f)                    // penetrated ground
        {
            t.pos.y = terrainHeight;
            t.velocity.y = 0.0f;
        }
        else if (heightDiff > 60.0f)                // lost contact
        {
            phys.onGround = false;
        }
        else
        {
            t.pos.y = std::lerp(t.pos.y, terrainHeight, 0.35f);   // soft follow
            t.velocity.y = std::min(t.velocity.y, 0.0f);
        }
    }
    else
    {
        // Landing detection
        if (t.velocity.y <= 0.0f && t.pos.y <= terrainHeight + 12.0f)
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

        const float groundSkin = 10.f;

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
    int terrainLayerIndex = 0;
    while (file >> str)
    {
        if (str == "Camera")
        {
            file >> m_cameraConfig.FOVY
                 >> m_cameraConfig.NEAR_PLANE
                 >> m_cameraConfig.FAR_PLANE;
        }
        if (str == "Player")
        {
            file >> m_playerConfig.MOVE_SPEED
                 >> m_playerConfig.ROTATION_SPEED
                 >> m_playerConfig.HEIGHT_OFFSET
                 >> m_playerConfig.EYE_OFFSET
                 >> m_playerConfig.POSITION_X
                 >> m_playerConfig.POSITION_Z;
        }
        if (str == "DateTimePlace")
        {
            file >> m_gameYear
                 >> m_gameMonth
                 >> m_gameDayOfMonth
                 >> m_gameTimeOfDay
                 >> m_latitude
                 >> m_longitude;
        }
    }
    std::srand(std::time(0));
    m_homeLocationXZ = hexToWorld(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z);
}

void Scene_IC_Camp::spawnPlayer()
{
    m_player = m_entityManager.addEntity("player");
    m_playerConfig.HEIGHT_OFFSET *= 100.f; // convert from metres to cm
    m_playerConfig.EYE_OFFSET *= 100.f;    // convert from metres to cm
    m_playerConfig.MOVE_SPEED *= 100.f;    // convert from m/s to cm/s
    m_playerConfig.ROTATION_SPEED = Astro::toRad(m_playerConfig.ROTATION_SPEED);
    sf::Vector2f playerPosition = hexToWorld(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z);
    sf::Vector3f spawnPos(playerPosition.x, heightAt(playerPosition.x, playerPosition.y), playerPosition.y);
    m_entityManager.addPlayer(m_player, CPlayer());
    m_entityManager.addTransform(m_player, CTransform3D(spawnPos));
    m_entityManager.addInput(m_player, CInput());
    m_entityManager.addPhysics(m_player, CPhysics());
    m_entityManager.addBob(m_player, CBob(1.0f, 6.0f, 5.5f));   // rate, vertical mag, lateral mag
    auto& phys = m_entityManager.getPhysics(m_player);
    phys.onGround = true;
}

void Scene_IC_Camp::spawnCamera()
{
    m_camera = m_entityManager.addEntity("camera");
    m_cameraConfig.FOVY = Astro::toRad(m_cameraConfig.FOVY);
    m_cameraConfig.NEAR_PLANE *= 100.f; // convert from metres to cm
    m_cameraConfig.FAR_PLANE  *= 100.f; // convert from metres to cm
    m_entityManager.addCamera(m_camera, CCamera(
        m_cameraConfig.FOVY,
        float(m_cameraConfig.VIEWPORT_WIDTH)/m_cameraConfig.VIEWPORT_HEIGHT,
        m_cameraConfig.NEAR_PLANE,
        m_cameraConfig.FAR_PLANE,
        sf::Vector2u(m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT)
        ));
    m_entityManager.addTransform(m_camera, CTransform3D());
}

void Scene_IC_Camp::spawnOrb(int hexQ, int hexR, const sf::Color& color, float radius, float bobRate, float bobMagnitude)
{
    auto orb = m_entityManager.addEntity("orb");
    sf::Vector2f worldPos = hexToWorld(hexQ, hexR);
    
    m_entityManager.addTransform(orb, CTransform3D(
        sf::Vector3f(worldPos.x, heightAt(worldPos.x, worldPos.y) + 100.0f, worldPos.y)
        ));
    
    m_entityManager.addOrb(orb, COrb(color, radius));
    m_entityManager.addBob(orb, CBob(bobRate, bobMagnitude, 0.0f));   // orbs only bob vertically
}

// World convention: Y is vertical (up). worldPos / hexToWorld() return XZ ground-plane coords.
void Scene_IC_Camp::spawnOrbFauna(int hexQ, int hexR, const sf::Color& color, float radius,
                                   float bobRate, float bobMagnitude,
                                   const CEyes& eyes)
{
    auto orb = m_entityManager.addEntity("orb");

    const sf::Vector2f groundXZ = hexToWorld(hexQ, hexR);
    const float        groundY  = heightAt(groundXZ.x, groundXZ.y);
    const float        hoverY   = groundY + bobMagnitude + radius;   // rests at its own full bob height

    m_entityManager.addTransform(orb, CTransform3D(
        sf::Vector3f(groundXZ.x, hoverY, groundXZ.y)
    ));
    m_entityManager.addOrb (orb, COrb(color, radius));
    m_entityManager.addBob (orb, CBob(bobRate, bobMagnitude, 0.0f));  // vertical bob only
    m_entityManager.addEyes(orb, eyes);
}

void Scene_IC_Camp::spawnDebugOrbs(int count)
{
    static const std::vector<sf::Color> palette = {
        sf::Color(240, 208,  96, 190),
        sf::Color(120, 194, 255, 170),
        sf::Color(255, 140,  80, 180),
        sf::Color(160, 255, 160, 175),
        sf::Color(220, 130, 255, 185),
    };

    static const std::vector<sf::Color> tapetumPalette = {
        sf::Color(255, 220,  80),   // golden
        sf::Color( 80, 255, 180),   // aqua
        sf::Color(255, 100, 100),   // red
    };

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> hexQDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> hexRDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> radiusDist(20.0f, 80.0f);
    std::uniform_real_distribution<float> bobRateDist(0.5f, 3.0f);
    std::uniform_real_distribution<float> bobMagDist(4.0f, 12.0f);
    std::uniform_real_distribution<float> gazeDeviationDist(-0.32f, 0.32f);
    std::uniform_real_distribution<float> dilationDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> closureDist(0.0f, 1.0f);   // mostly open
    std::uniform_int_distribution<int>    tapetumChance(0, 2);        // 1 in 3 have tapetum
    std::uniform_int_distribution<int>    tapetumColorIdx(0, static_cast<int>(tapetumPalette.size()) - 1);

    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 16);
        }
    };
    std::unordered_set<std::pair<int,int>, PairHash> usedCoords;

    int spawned     = 0;
    int maxAttempts = count * 10;
    int attempts    = 0;

    while (spawned < count && attempts < maxAttempts)
    {
        ++attempts;
        int hexQ = static_cast<int>(hexQDist(rng));
        int hexR = static_cast<int>(hexRDist(rng));
        if (!usedCoords.insert({hexQ, hexR}).second) continue;

        const sf::Color& color = palette[spawned % palette.size()];
        float radius  = radiusDist(rng);
        float bobRate = bobRateDist(rng);
        float bobMag  = bobMagDist(rng);

        // Random gaze direction, normalised in XZ — orbs look around horizontally
        sf::Vector3f baseForward = {0.0f, 0.0f, 1.0f};
        sf::Vector3f gazeOffset = { gazeDeviationDist(rng), gazeDeviationDist(rng), 0.0f };
        sf::Vector3f gaze = Camera::normalize(baseForward + gazeOffset);
        // sf::Vector3f gaze = Camera::normalize({ gazeDeviationDist(rng), 0.0f, gazeDeviationDist(rng) });;
        bool         hasTapetum  = tapetumChance(rng) == 0;
        sf::Color    tapetumCol  = tapetumPalette[tapetumColorIdx(rng)];
        CEyes eyes(gaze, dilationDist(rng), closureDist(rng), hasTapetum, tapetumCol);
        spawnOrbFauna(hexQ, hexR, color, radius, bobRate, bobMag, eyes);
        auto& eyesComp = m_entityManager.getEyes(m_entityManager.getEntities().back());
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

    sf::Vector3f forward = forwardFromTransform(playerTransform);
    forward = Camera::normalize(forward);
    sf::Vector3f right(forward.z, 0.f, -forward.x);

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
    float lateralAmplitude = 5.5f + (3.8f - 5.5f) * t;
    float verticalAmplitude = 6.2f + (4.8f - 6.2f) * t;

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

    camTransform.pos = headPos - (forward * m_playerConfig.EYE_OFFSET) + m_cameraBobOffset;
    camTransform.yaw   = playerTransform.yaw;
    camTransform.pitch = playerTransform.pitch;

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

void Scene_IC_Camp::initializeBakeFBO() {
    sf::Vector2u windowSize = m_game.window().getSize();

    // Create the float texture to store XZ world coordinates
    glGenTextures(1, &m_bakeColorTex);
    glBindTexture(GL_TEXTURE_2D, m_bakeColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                 windowSize.x, windowSize.y,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth renderbuffer so depth testing works during the bake
    glGenRenderbuffers(1, &m_bakeDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_bakeDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          windowSize.x, windowSize.y);

    // Assemble the FBO
    glGenFramebuffers(1, &m_bakeFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bakeFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_bakeColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_bakeDepthRBO);

    // Verify it's complete before leaving
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // You can hook this into whatever logging you use
        std::cerr << "Bake FBO incomplete: 0x" << std::hex << status << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene_IC_Camp::debugDumpBakeTexture() {
    sf::Vector2u windowSize = m_game.window().getSize();
    std::vector<float> pixels(windowSize.x * windowSize.y * 4);
    
    glBindTexture(GL_TEXTURE_2D, m_bakeColorTex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Sample a few pixels from the centre of the texture
    int cx = windowSize.x / 2;
    int cy = windowSize.y / 2;
    int idx = (cy * windowSize.x + cx) * 4;
    
    std::cout << "Bake centre pixel: "
              << pixels[idx+0] << ", "  // should be world X
              << pixels[idx+1] << ", "  // should be world Z
              << pixels[idx+2] << ", "  // 0.0
              << pixels[idx+3] << "\n"; // 1.0 if terrain, 0.0 if sky
}

void Scene_IC_Camp::runBakePass() {
    auto& transform  = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    auto viewMatrix = Camera::getViewMatrix(transform);
    auto projMatrix = Camera::getProjectionMatrix(cameraData);

    sf::Vector2u windowSize = m_game.window().getSize();

    glBindFramebuffer(GL_FRAMEBUFFER, m_bakeFBO);
    glViewport(0, 0, windowSize.x, windowSize.y);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_bakeProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_topdownTexture.getNativeHandle());
    glUniform1i(glGetUniformLocation(m_bakeProgram, "topoTopdownTex"), 0);
    glUniform2f(glGetUniformLocation(m_bakeProgram, "topdownWorldMin"),
                m_topdownWorldMin.x, m_topdownWorldMin.y);
    glUniform2f(glGetUniformLocation(m_bakeProgram, "topdownWorldSize"),
                m_topdownWorldSize.x, m_topdownWorldSize.y);
    glUniform1f(glGetUniformLocation(m_bakeProgram, "topdownHeightMax"),
                m_topdownMaxHeight);
    glUniformMatrix4fv(glGetUniformLocation(m_bakeProgram, "u_View"),
                       1, GL_FALSE, viewMatrix.data());
    glUniformMatrix4fv(glGetUniformLocation(m_bakeProgram, "u_Projection"),
                       1, GL_FALSE, projMatrix.data());

    glBindVertexArray(m_gridVAO);
    glDrawElements(GL_TRIANGLES, m_gridIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
    const float worldRadius = 25600.f;

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

void Scene_IC_Camp::uploadOrbBatchToShader(sf::Shader& shader, const OrbBatch& batch,
                                           const sf::Vector3f& sunDirView)
{
    int size = static_cast<int>(batch.centersView.size());
    shader.setUniform("u_batchSize", size);

    if (size > 0)
    {
        shader.setUniformArray("u_orbCenterView",   batch.centersView.data(),     size);
        shader.setUniformArray("u_orbColor",        batch.colors.data(),          size);
        shader.setUniformArray("u_orbDepthNorm",    batch.depthNorms.data(),      size);
        shader.setUniformArray("u_quadOrigin",      batch.quadOrigins.data(),     size);
        shader.setUniformArray("u_texSize",         batch.texSizes.data(),        size);
        shader.setUniformArray("u_quadSize",        batch.quadSizes.data(),       size);
        shader.setUniformArray("u_gazeDir",         batch.gazes.data(),           size);
        shader.setUniformArray("u_orbForward",      batch.forwards.data(),        size);
        shader.setUniformArray("u_hasTapetum",      batch.hasTapetums.data(),     size);
        shader.setUniformArray("u_tapetumColor",    batch.tapetumColors.data(),   size);
        shader.setUniformArray("u_pupilDilation",   batch.pupilDilations.data(),  size);
        shader.setUniformArray("u_eyelidClosure",   batch.eyelidClosures.data(),  size);
    }
    auto& transform = m_entityManager.getTransform(m_camera);
    shader.setUniform("cameraPos", transform.pos);
    shader.setUniform("topdownWorldMin", sf::Glsl::Vec2(m_topdownWorldMin.x, m_topdownWorldMin.y));
    shader.setUniform("topdownWorldSize", sf::Glsl::Vec2(m_topdownWorldSize.x, m_topdownWorldSize.y));
    shader.setUniform("topdownHeightMax", m_topdownMaxHeight);
    shader.setUniform("heightMap", m_topdownTexture);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    shader.setUniform("nearPlane", cameraData.nearPlane);
    shader.setUniform("farPlane", cameraData.farPlane);
    shader.setUniform("sunDir", sf::Glsl::Vec3(sunDirView.x, sunDirView.y, sunDirView.z));
    shader.setUniform("sunDirWorld", m_astroState.sunDirection);
    shader.setUniform("sunColor", m_astroState.sunColor);
    shader.setUniform("u_viewportSize", sf::Glsl::Vec2(
        static_cast<float>(m_bakeTexture.getSize().x),
        static_cast<float>(m_bakeTexture.getSize().y)));

    shader.setUniform("headlampEnabled", shouldHeadlightsBeOn() ? 1.0f : 0.0f);
    shader.setUniform("headlampIntensity", 2.5f);
    shader.setUniform("headlampRange", 8500.0f);
    shader.setUniform("headlampConeCos", 1.0f);
}

void Scene_IC_Camp::renderDemoSphere()
{
    auto& window       = m_game.window();
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData      = m_entityManager.getCamera(m_camera);

    sf::Vector3f camFwd   = -Camera::getForward(camTransform);
    sf::Vector3f camRight = Camera::getRight(camTransform);
    sf::Vector3f camUp    = Camera::getUp(camTransform);
    sf::Vector3f sunDir   = m_astroState.sunDirection;

    // Hardcoded demo orb
    sf::Vector3f orbCentre  = { 0.f, heightAt(0.0f, -500.0f) + 150.f, -500.f };
    float        orbRadius  = 150.f;
    sf::Vector3f orbForward = {  0.f, 0.f, -1.f }; // South
    sf::Vector3f orbRight   = {  1.f, 0.f,  0.f }; // West
    sf::Vector3f orbUp      = {  0.f, 1.f,  0.f }; // Up

    // ==================== SFML STATE HANDOFF ====================
    window.setActive(true);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // draw to window
    sf::Vector2u windowSize = m_game.window().getSize();
    glViewport(0, 0, windowSize.x, windowSize.y);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint prog = m_demoSphereProgram;
    glUseProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "DemoSphere link error: " << log << std::endl;
    }
    
    // Camera uniforms
    glUniform2f(glGetUniformLocation(prog, "u_viewportSize"),
        static_cast<float>(camData.viewportSize.x),
        static_cast<float>(camData.viewportSize.y));
    glUniform1f(glGetUniformLocation(prog, "u_fovY"),          camData.fovY);
    glUniform3f(glGetUniformLocation(prog, "u_cameraPos"),     camTransform.pos.x, camTransform.pos.y, camTransform.pos.z);
    glUniform3f(glGetUniformLocation(prog, "u_cameraForward"), camFwd.x,   camFwd.y,   camFwd.z);
    glUniform3f(glGetUniformLocation(prog, "u_cameraRight"),   camRight.x, camRight.y, camRight.z);
    glUniform3f(glGetUniformLocation(prog, "u_cameraUp"),      camUp.x,    camUp.y,    camUp.z);
    glUniform3f(glGetUniformLocation(prog, "u_sunDir"),        sunDir.x,   sunDir.y,   sunDir.z);

    // Orb uniforms
    glUniform3f(glGetUniformLocation(prog, "u_orbCentre"),  orbCentre.x,  orbCentre.y,  orbCentre.z);
    glUniform1f(glGetUniformLocation(prog, "u_orbRadius"),  orbRadius);
    glUniform3f(glGetUniformLocation(prog, "u_orbForward"), orbForward.x, orbForward.y, orbForward.z);
    glUniform3f(glGetUniformLocation(prog, "u_orbRight"),   orbRight.x,   orbRight.y,   orbRight.z);
    glUniform3f(glGetUniformLocation(prog, "u_orbUp"),      orbUp.x,      orbUp.y,      orbUp.z);

    // Full screen quad
    static const float quadVerts[] = {
        -1.f, -1.f,
         1.f, -1.f,
         1.f,  1.f,
        -1.f, -1.f,
         1.f,  1.f,
        -1.f,  1.f,
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ==================== CLEANUP RESTORATION ====================
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    // =============================================================
}

void Scene_IC_Camp::renderOrbs()
{
    if (m_orbDrawItemCount == 0) return;

    auto& window = m_game.window();
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData = m_entityManager.getCamera(m_camera);

    sf::Vector3f sunDirView = Camera::worldToCamera(
        m_astroState.sunDirection, camTransform.pitch, camTransform.yaw, camTransform.roll);

    constexpr int BATCH_SIZE = 64;
    OrbBatch batch;
    batch.reserve(BATCH_SIZE);
    sf::VertexArray vertices(sf::PrimitiveType::Triangles, 0);

    for (int i = 0; i < m_orbDrawItemCount; ++i)
    {
        float padding = 1.5f;
        const auto& item = m_orbDrawItems[i];
        const float r = item.radiusPx;
        const sf::Vector2f origin(item.screenPos.x - r * padding, item.screenPos.y - r * padding);
        const sf::Vector2f size(r * 2.f, r * 2.f);
        const sf::Vector2f expandedSize = size * padding; // expand the quad to remove clipping

        uint8_t idx = static_cast<uint8_t>(batch.centersView.size());
        sf::Color indexColor(idx, 0, 0, 255);

        sf::Vertex v1(origin, indexColor);
        sf::Vertex v2({origin.x + expandedSize.x, origin.y}, indexColor);
        sf::Vertex v3({origin.x + expandedSize.x, origin.y + expandedSize.y}, indexColor);
        sf::Vertex v4({origin.x, origin.y + expandedSize.y}, indexColor);

        vertices.append(v1);
        vertices.append(v2);
        vertices.append(v3);
        vertices.append(v1);
        vertices.append(v3);
        vertices.append(v4);

        batch.centersView.push_back(item.cameraSpacePos);
        batch.colors.emplace_back(
            item.color.r / 255.f, item.color.g / 255.f,
            item.color.b / 255.f, item.color.a / 255.f);
        batch.depthNorms.push_back(item.distNorm);
        batch.quadOrigins.push_back(origin);
        batch.texSizes.emplace_back(size);
        batch.quadSizes.push_back(expandedSize);
        batch.gazes.push_back(item.gazeDirection);
        batch.forwards.push_back(item.forward);
        batch.hasTapetums.push_back(item.hasTapetum);
        batch.tapetumColors.push_back(item.tapetumColor);
        batch.pupilDilations.push_back(item.pupilDilation);
        batch.eyelidClosures.push_back(item.eyelidClosure);

        if (batch.centersView.size() == BATCH_SIZE || i == m_orbDrawItemCount - 1)
        {
            uploadOrbBatchToShader(m_orbShader, batch, sunDirView);
            
            sf::RenderStates states;
            states.shader = &m_orbShader;

            // ==================== INJECTION POINT: HARDWARE TEXTURE LOCKS ====================
            // 1. Force bind the shader program so we can query its uniform locations
            sf::Shader::bind(&m_orbShader);
            GLuint prog = m_orbShader.getNativeHandle();

            // 2. Bind the Bake Texture (Texture coordinates map) to Unit 2
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, m_bakeColorTex);
            glUniform1i(glGetUniformLocation(prog, "bakeTex"), 2);

            // 3. Bind the raw Heightmap Texture to Unit 3 (In case your orbs want to sample real heights)
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, m_topdownTexture.getNativeHandle());
            glUniform1i(glGetUniformLocation(prog, "heightMap"), 3);
            // =================================================================================

            // SFML draws the vertex quad array using our state-locked texture configurations
            window.draw(vertices, states);

            // ==================== INJECTION POINT: CLEANUP RESTORATION ====================
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, 0);
            
            glUseProgram(0);
            sf::Shader::bind(nullptr);
            sf::Texture::bind(nullptr);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            // =================================================================================

            vertices.clear();
            batch.clear();
        }
    }
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
    m_renderTexture.clear(sf::Color::Transparent);
    m_renderTexture.display();
    m_skyTexture.clear(sf::Color::Transparent);
    m_skyTexture.draw(skyQuad, &m_sky);
    m_skyTexture.setSmooth(true);
    m_skyTexture.display();
}

void Scene_IC_Camp::runTerrainPass(const sf::Glsl::Mat3& worldToCamMatrix) {
    auto& transform  = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    sf::Vector2u winSize = m_game.window().getSize();
    sf::Vector3f worldPos = screenToWorld(sf::Mouse::getPosition(m_game.window()));
    sf::Vector2i hex = worldToHex(worldPos.x, worldPos.z);
    const bool headlampOn = shouldHeadlightsBeOn();

    // 1. Clear target natively.
    m_renderTexture.clear(sf::Color::Transparent);

    // 2. Safely apply uniforms
    m_terrainShader.setUniform("viewportSize",     sf::Glsl::Vec2(winSize.x, winSize.y));
    m_terrainShader.setUniform("m_hexSize",        m_hexSize);
    m_terrainShader.setUniform("cameraPos",        sf::Glsl::Vec3(transform.pos.x, transform.pos.y, transform.pos.z));
    m_terrainShader.setUniform("camHeight",        getCameraHeightAboveGround(transform.pos));
    m_terrainShader.setUniform("heightMap",        m_topdownTexture);
    m_terrainShader.setUniform("u_heightMax",      m_topdownMaxHeight);
    m_terrainShader.setUniform("farPlane",         cameraData.farPlane);
    m_terrainShader.setUniform("worldToCamMatrix", worldToCamMatrix);
    m_terrainShader.setUniform("topdownWorldMin",  sf::Glsl::Vec2(m_topdownWorldMin.x, m_topdownWorldMin.y));
    m_terrainShader.setUniform("topdownWorldSize", sf::Glsl::Vec2(m_topdownWorldSize.x, m_topdownWorldSize.y));
    m_terrainShader.setUniform("sunDir",           m_astroState.sunDirection);
    m_terrainShader.setUniform("sunColor",         m_astroState.sunColor);
    m_terrainShader.setUniform("ambientStrength",  0.3f);
    m_terrainShader.setUniform("gridColor",        colorToShader(m_gridColor));
    m_terrainShader.setUniform("headlampOn",       headlampOn);
    m_terrainShader.setUniform("headlampIntensity",4.f);
    m_terrainShader.setUniform("headlampColor",    colorToShader(sf::Color(255, 244, 214)));
    m_terrainShader.setUniform("headlampRange",    15000.0f);
    m_terrainShader.setUniform("u_heightMax",      m_topdownMaxHeight);
    m_terrainShader.setUniform("u_reliefExaggeration", 1.0f);
    m_terrainShader.setUniform("cursorMode",       m_cursorMode);
    m_terrainShader.setUniform("hoveredHex",       sf::Glsl::Vec2((float)hex.x, (float)hex.y));

    // 3. Create the screen-space quad matching pixel space dimensions
    sf::VertexArray screenQuad(sf::PrimitiveType::TriangleStrip, 4);
    screenQuad[0] = sf::Vertex({0.f, 0.f},                 sf::Color::White, {0.f, 1.f});
    screenQuad[1] = sf::Vertex({0.f, (float)winSize.y},    sf::Color::White, {0.f, 0.f});
    screenQuad[2] = sf::Vertex({(float)winSize.x, 0.f},    sf::Color::White, {1.f, 1.f});
    screenQuad[3] = sf::Vertex({(float)winSize.x, (float)winSize.y}, sf::Color::White, {1.f, 0.f});

    // 4. Bind the shader program via SFML
    sf::Shader::bind(&m_terrainShader);
    GLuint prog = m_terrainShader.getNativeHandle();

    // 5. Force-bind textures to concrete units right before execution
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_bakeColorTex);
    glUniform1i(glGetUniformLocation(prog, "bakeTex"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_topdownTexture.getNativeHandle());
    glUniform1i(glGetUniformLocation(prog, "heightMap"), 3);

    // State tweaks for the screen quad:
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND); // Disabled only for the duration of this draw call

    // 6. Draw the screen quad using our shader
    m_renderTexture.draw(screenQuad, &m_terrainShader);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 7. CLEANUP STATE RESTORATION
    glUseProgram(0);
    glEnable(GL_BLEND); // Re-enable alpha blending globally for subsequent UI / text steps
    sf::Shader::bind(nullptr);
    sf::Texture::bind(nullptr);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_renderTexture.display();
}

// =========================================================================
// Orb & Shadow Pipelines
// =========================================================================

void Scene_IC_Camp::sortOrbs()
{
    m_orbDrawItemCount = 0;

    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData      = m_entityManager.getCamera(m_camera);

    const sf::Vector2u winSize       = m_bakeTexture.getSize();
    const float        focalLengthPx = (winSize.y * 0.5f) / std::tan(camData.fovY * 0.5f);

    for (auto orb : m_entityManager.getEntities("orb"))
    {
        if (!m_entityManager.hasOrb(orb) || !m_entityManager.hasTransform(orb)) continue;

        auto& orbTransform = m_entityManager.getTransform(orb);
        auto& orbData      = m_entityManager.getOrb(orb);

        // World convention: Y is vertical. XZ is the ground plane.
        sf::Vector3f relative    = orbTransform.pos - camTransform.pos;
        sf::Vector3f cameraSpace = Camera::worldToCamera(
            relative, camTransform.pitch, camTransform.yaw, camTransform.roll);

        if (cameraSpace.z >= -camData.nearPlane) continue;

        sf::Vector2f screenPos;
        if (!Camera::worldToScreen(camTransform, camData, orbTransform.pos, screenPos)) continue;

        const float dist = std::sqrt(relative.x * relative.x +
                                     relative.y * relative.y +
                                     relative.z * relative.z);

        const float radiusPx = orbData.radius * focalLengthPx / dist;
        if (radiusPx <= 0.5f) continue;

        const float depthNorm = std::clamp(
            (dist - camData.nearPlane) / (camData.farPlane - camData.nearPlane), 0.0f, 1.0f);

        // Compute camera forward vector for eye gaze projection in the shader
        sf::Vector3f testForward(0.0f, 0.0f, -1.0f);
        sf::Vector3f forward = Camera::worldToCamera(testForward, camTransform.pitch, camTransform.yaw, camTransform.roll);
        forward.y = 0.0f;
        forward = Camera::normalize(forward);

        // Eye data — use component if present, otherwise sensible eyeless defaults
        sf::Vector3f gazeDirection  = { 0.0f, 0.0f, 1.0f };
        float        pupilDilation  = 0.5f;
        float        eyelidClosure  = 0.0f;
        float        hasTapetum     = 0.0f;
        sf::Vector3f tapetumColor   = { 1.0f, 1.0f, 1.0f };

        if (m_entityManager.hasEyes(orb))
        {
            const auto& eyes = m_entityManager.getEyes(orb);
            gazeDirection = eyes.gazeDirection;
            pupilDilation = eyes.pupilDilation;
            eyelidClosure = eyes.eyelidClosure;
            hasTapetum    = eyes.hasTapetum ? 1.0f : 0.0f;
            tapetumColor  = { eyes.tapetumColor.r / 255.0f,
                              eyes.tapetumColor.g / 255.0f,
                              eyes.tapetumColor.b / 255.0f };
        }

        m_orbDrawItems[m_orbDrawItemCount++] = {
            screenPos,
            cameraSpace,
            radiusPx,
            orbData.radius,
            gazeDirection,
            forward,
            hasTapetum,
            tapetumColor,
            pupilDilation,
            eyelidClosure,
            orbTransform.pos,
            dist,
            depthNorm,
            orbData.color
        };
    }

    std::sort(m_orbDrawItems.begin(), m_orbDrawItems.begin() + m_orbDrawItemCount,
        [](const OrbDrawItem& a, const OrbDrawItem& b) {
            return a.distSort > b.distSort;
        });
}

void Scene_IC_Camp::updateShadowOrbs()
{
    m_shadowOrbList.clear();
    constexpr int   MAX_SHADOW_ORBS    = 64;
    constexpr float SHADOW_CUTOFF_DIST = 5000.0f;

    auto& camTransform = m_entityManager.getTransform(m_camera);

    for (int i = m_orbDrawItemCount - 1; i >= 0 && (int)m_shadowOrbList.size() < MAX_SHADOW_ORBS; --i)
    {
        const auto& item = m_orbDrawItems[i];
        if (item.distSort > SHADOW_CUTOFF_DIST) continue;

        m_shadowOrbList.push_back({ item.worldPos, item.orbRadius });
    }
}

void Scene_IC_Camp::uploadShadowOrbsToShader(sf::Shader& shader)
{
    int count = static_cast<int>(m_shadowOrbList.size());
    shader.setUniform("u_shadowOrbCount", count);
    shader.setUniform("u_shadowDarkness", 0.6f);

    for (int i = 0; i < count; ++i)
    {
        std::string idx = "[" + std::to_string(i) + "]";
        shader.setUniform("u_shadowOrbPos" + idx,
            sf::Glsl::Vec3(m_shadowOrbList[i].worldPos.x,
                           m_shadowOrbList[i].worldPos.y,
                           m_shadowOrbList[i].worldPos.z));
        shader.setUniform("u_shadowOrbRadius" + idx, m_shadowOrbList[i].radius);
    }
}

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

sf::Vector2i Scene_IC_Camp::worldToHex(float x, float z) const {
    float q = (2.f/3.f * x) / m_hexSize;
    float r = (z / (m_hexSize * std::sqrt(3.f))) - q / 2.f;
    
    // Round to nearest hex using cube coordinate rounding
    float s = -q - r;
    int rq = int(std::round(q));
    int rr = int(std::round(r));
    int rs = int(std::round(s));
    float dq = std::abs(rq - q);
    float dr = std::abs(rr - r);
    float ds = std::abs(rs - s);
    if (dq > dr && dq > ds)      rq = -rr - rs;
    else if (dr > ds)             rr = -rq - rs;
    
    return sf::Vector2i(rq, rr);
}

sf::Vector2f Scene_IC_Camp::hexToWorld(int q, int r) const {
    float x = m_hexSize * 3.f/2.f * q;
    float z = m_hexSize * std::sqrt(3.f) * (r + q/2.f);
    return sf::Vector2f(x, z);
}

// Interesting.  This is ripe for replacement now that we have a new system.
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

    sf::Vector3f rayDir = Camera::cameraToWorld(sf::Vector3f(x_ndc * f * aspectRatio, y_ndc * f, -1.0f), cam.pitch, cam.yaw, cam.roll);
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