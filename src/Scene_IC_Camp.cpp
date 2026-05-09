#include "Scene_IC_Camp.h"
#include "GameEngine.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <cmath>
#include "imgui.h"
#include "imgui-SFML.h"
#include "Camera.h"

static sf::Glsl::Mat3 toGlslMat3(const std::array<std::array<float, 3>, 3>& matrix) {
    const float flattened[9] = {
        matrix[0][0], matrix[1][0], matrix[2][0],
        matrix[0][1], matrix[1][1], matrix[2][1],
        matrix[0][2], matrix[1][2], matrix[2][2]
    };

    return sf::Glsl::Mat3(flattened);
}

Scene_IC_Camp::Scene_IC_Camp(GameEngine& game, const std::string& levelPath)
    : Scene(game)
    , m_levelPath(levelPath)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    m_renderTexture = sf::RenderTexture({game.window().getSize().x, game.window().getSize().y});
    m_skyTexture = sf::RenderTexture({game.window().getSize().x, game.window().getSize().y});
    sf::Vector2u bakeSize(
        std::max(1u, game.window().getSize().x / 1),
        std::max(1u, game.window().getSize().y / 1)
    );
    m_bakeTexture = sf::RenderTexture({bakeSize.x, bakeSize.y});
    m_bakeTexture.setSmooth(false);
    m_topdownTexture = sf::RenderTexture({m_topdownTextureSize, m_topdownTextureSize});
    m_topdownTexture.setSmooth(false);
    m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});
    m_gridColor = Theme::color("cerulean");
    m_cameraConfig.VIEWPORT_WIDTH = game.window().getSize().x;
    m_cameraConfig.VIEWPORT_HEIGHT = game.window().getSize().y;
    loadLevel(m_levelPath);
    spawnCamera();
    spawnPlayer();
    initializeTerrainLayers();
    m_topdownMaxHeight = computeSceneMaxHeight();
    buildHud();
    updateHUDData();
    updateSunPosition();
    m_game.setMouseCaptured(true);
    m_cursorMode = false;
}

void Scene_IC_Camp::updateCamera() {
    auto& camTransform = m_camera->get<CTransform3D>();
    auto& playerTransform = m_player->get<CTransform3D>();
    auto& playerState = m_player->get<CPlayer>();
    auto& cameraData = m_camera->get<CCamera>();
    
    sf::Vector3f headPos = playerTransform.pos;
    
    sf::Vector3f forward = Camera::getForwardXZ(m_player);
    sf::Vector3f right(forward.z, 0.f, -forward.x);

    float horizontalSpeed = std::sqrt(
        playerTransform.velocity.x * playerTransform.velocity.x +
        playerTransform.velocity.z * playerTransform.velocity.z
    );

    float moveFactor = std::clamp(horizontalSpeed / std::max(m_playerConfig.MOVE_SPEED, 0.0001f), 0.0f, 1.0f);
    float phase = playerState.bobAccumulator * 6.2831853f;

    // Slightly slower cadence: previously felt too aggressive — slow by factor ~3
    float cadenceScale = 1.0f; // bobAccumulator already slowed in sMovement

    // Slightly tighten lateral motion to reduce perceived sway
    float lateralAmplitude = playerState.sprinting ? 4.2f : 6.8f;
    const float lateralScale = 0.85f; // 0.85 = 15% tighter laterally
    lateralAmplitude *= lateralScale;
    float verticalAmplitude = playerState.sprinting ? 7.4f : 6.2f;

    float lateralBob = std::sin(phase * cadenceScale) * lateralAmplitude * moveFactor;
    float verticalBob = std::sin(phase * 2.f * cadenceScale) * verticalAmplitude * moveFactor;

    // Target bob offset in world-space (right axis and vertical)
    sf::Vector3f targetBob = right * lateralBob + sf::Vector3f(0.f, verticalBob, 0.f);

    // Smooth/lag the camera bob so the world feels a bit more inertial
    m_cameraBobOffset += (targetBob - m_cameraBobOffset) * m_bobLag;

    camTransform.pos = headPos - (forward * m_playerConfig.EYE_OFFSET) + m_cameraBobOffset;

    camTransform.yaw   = playerTransform.yaw;
    camTransform.pitch = playerTransform.pitch;

    float targetFov = m_cameraConfig.FOVY + (playerState.sprinting ? 0.14f : 0.0f);
    cameraData.fovY += (targetFov - cameraData.fovY) * 0.12f;
}

void Scene_IC_Camp::updateHUDData()
{
    // Use a constant for clarity
    const float RAD_TO_DEG = 180.0f / 3.14159265f;

    sf::Vector3f currentLocation = m_player->get<CTransform3D>().pos;
    sf::Vector3f forward = Camera::getForwardXZ(m_player);

    float currentHeading = -std::atan2(forward.x, forward.z) * RAD_TO_DEG;

    updateMinimapTexture();

    m_hudData.position = currentLocation;
    m_hudData.homeLocation = sf::Vector2f(m_homeLocationXZ.x, m_homeLocationXZ.y);
    m_hudData.cameraYaw = currentHeading;
    m_hudData.headlightState = static_cast<int>(m_headlightState);
    m_hudData.headlightEnabled = shouldHeadlightsBeOn();
    m_hudData.minimapTex = &m_minimapTexture.getTexture();
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
        return m_sunDirection.y < 0.12f || m_sunIntensity < 0.75f;
    }
}

void Scene_IC_Camp::updateMinimapTexture()
{
    if (m_minimapTexture.getSize().x != m_minimapTextureSize ||
        m_minimapTexture.getSize().y != m_minimapTextureSize)
    {
        m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});
    }

    const sf::Vector3f playerPos = m_player->get<CTransform3D>().pos;
    const float texSize  = static_cast<float>(m_minimapTextureSize);  // 256
    const float center   = texSize * 0.5f;
    const float worldRadius = 12800.f;

    // ── Draw the hillshaded topo layer via shader ──────────────────────────
    m_minimapTexture.clear(sf::Color::Transparent);

    sf::RectangleShape fullQuad({texSize, texSize});

    m_topoMinimapShader.setUniform("u_playerXZ",
        sf::Glsl::Vec2(playerPos.x, playerPos.z));
    m_topoMinimapShader.setUniform("u_worldRadius",  worldRadius);
    m_topoMinimapShader.setUniform("u_texSize",      texSize);
    m_topoMinimapShader.setUniform("u_heightMax",    m_topdownMaxHeight);
    m_topoMinimapShader.setUniform("u_reliefExaggeration", 1.0f);
    Scene_IC_Camp::uploadTerrainLayersToShader(m_topoMinimapShader, "u_layers");
    for (int i = 0; i < 16; ++i) {
        m_topoMinimapShader.setUniform("u_activeLayerEnabled[" + std::to_string(i) + "]", 1.0f);
    }
    sf::RenderStates states;
    states.shader = &m_topoMinimapShader;
    m_minimapTexture.draw(fullQuad, states);

    // ── Home marker (unchanged) ────────────────────────────────────────────
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

void Scene_IC_Camp::buildHud()
{
    m_hud = std::make_unique<HUD>(m_game.window().getSize());
}

HUD* Scene_IC_Camp::getHUD() const
{
    return m_hud.get();
}

void Scene_IC_Camp::loadLevel(const std::string& filename)
{
    m_entityManager = EntityManager();

    std::ifstream file(filename);
    std::string str;
    while (file.good())
    {
        file >> str;

        if (str == "Camera")
        {
            file >> m_cameraConfig.PITCH
                 >> m_cameraConfig.YAW
                 >> m_cameraConfig.ROLL
                 >> m_cameraConfig.FOVY
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
            file >> m_gameTimeOfDay
                 >> m_gameDayOfYear
                 >> m_latitude;
        }
    }
    std::srand(std::time(0));
    m_homeLocationXZ = sf::Vector2f(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z);
}

void Scene_IC_Camp::spawnPlayer()
{
    m_player = m_entityManager.addEntity("player");
    m_player->add<CPlayer>();
    m_player->add<CTransform3D>(
        sf::Vector3f(m_playerConfig.POSITION_X, Scene_IC_Camp::heightAt(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z), m_playerConfig.POSITION_Z),
        sf::Vector3f(0.f, 0.f, 0.f),
        sf::Vector3f(1.f, 1.f, 1.f),
        0.f, 0.f, 0.f
    );
    m_player->add<CInput>();
}

void Scene_IC_Camp::spawnCamera()
{
    m_camera = m_entityManager.addEntity("camera");
    m_camera->add<CCamera>(
        m_cameraConfig.FOVY,
        float(m_cameraConfig.VIEWPORT_WIDTH)/m_cameraConfig.VIEWPORT_HEIGHT,
        m_cameraConfig.NEAR_PLANE,
        m_cameraConfig.FAR_PLANE,
        sf::Vector2u(m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT)
    );
    m_camera->add<CTransform3D>(
        sf::Vector3f(m_cameraConfig.POSITION_X, m_cameraConfig.POSITION_Y, m_cameraConfig.POSITION_Z),
        sf::Vector3f(0.f, 0.f, 0.f),
        sf::Vector3f(1.f, 1.f, 1.f),
        m_cameraConfig.PITCH,
        m_cameraConfig.YAW,
        m_cameraConfig.ROLL
    );
}

void Scene_IC_Camp::onEnter() {
    m_player->get<CInput>().mouseDelta = {0.f, 0.f};
    sf::Vector2u size = m_game.window().getSize();
    sf::Mouse::setPosition(
        sf::Vector2i(size.x / 2, size.y / 2), 
        m_game.window()
    );
    m_game.setMouseCaptured(true);
}

void Scene_IC_Camp::onExit() {
    // TODO: Called when exiting the scene
}

void Scene_IC_Camp::onEnd() {
    // TODO: Called when ending the scene
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

    sMovement(dt);
    updateHUDData();
    updateSunPosition();
    updateCamera();
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
    auto& input = m_player->get<CInput>();  // slaved to player now, not camera

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
            if (m_cursorMode) {
                captureBake();
            }
            sf::Vector2u size = m_game.window().getSize();
            m_player->get<CInput>().mouseDelta = {0.f, 0.f};
            sf::Mouse::setPosition(
                sf::Vector2i(size.x / 2, size.y / 2), 
                m_game.window()
            );
        }
        else if (m_cursorMode) { return; }

        // Movement
        else if (action.name() == InputAction::MoveForward)  { input.forward  = true; }
        else if (action.name() == InputAction::MoveBackward) { input.backward = true; }
        else if (action.name() == InputAction::MoveLeft)     { input.left     = true; }
        else if (action.name() == InputAction::MoveRight)    { input.right    = true; }
        else if (action.name() == InputAction::Strafe)       { input.strafe   = true; }
        else if (action.name() == InputAction::Jump)         { input.jump     = true; }
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

        // Movement
        if      (action.name() == InputAction::MoveForward)  { input.forward  = false; }
        else if (action.name() == InputAction::MoveBackward) { input.backward = false; }
        else if (action.name() == InputAction::MoveLeft)     { input.left     = false; }
        else if (action.name() == InputAction::MoveRight)    { input.right    = false; }
        else if (action.name() == InputAction::Strafe)       { input.strafe   = false; }
        else if (action.name() == InputAction::Jump)         { input.jump     = false; }
        else if (action.name() == InputAction::Sprint)       { input.sprint   = false; }
        else if (action.name() == InputAction::Interact)     { input.interact = false; }
    }
}

void Scene_IC_Camp::sMovement(float dt)
{
    for (auto e : m_entityManager.getEntities())
    {
        if (!e->has<CTransform3D>() || !e->has<CInput>()) continue;

        auto& transform = e->get<CTransform3D>();
        auto& input = e->get<CInput>();

        if (e->tag() == "player")
        {
            auto& playerState = e->get<CPlayer>();

            // 1. Determine Speeds (Sprint on SHIFT)
            // Level file values are now interpreted as units-per-second.
            float currentMoveSpeed = input.sprint ? (m_playerConfig.MOVE_SPEED * 3.0f) : m_playerConfig.MOVE_SPEED;
            float currentRotSpeed  = m_playerConfig.ROTATION_SPEED;
            playerState.sprinting = input.sprint;
            playerState.moveSpeed = currentMoveSpeed;
            playerState.rotSpeed = currentRotSpeed;

            // 2. Handle Rotation (Turn with A/D unless CTRL is held)
            if (input.strafe)
            {
                if (input.left)  transform.yaw += currentRotSpeed * dt; 
                if (input.right) transform.yaw -= currentRotSpeed * dt;
            }

            // 3. Handle Mouse Look (Incremental deltas)
            transform.yaw   -= input.mouseDelta.x * 0.002f; // Sensitivity constant (mouse already delta/time-normalized)
            transform.pitch -= input.mouseDelta.y * 0.002f; 
            input.mouseDelta = { 0.f, 0.f }; // Consume the delta

            // 4. Translation Directions
            // Using your existing Camera utility to get forward vector based on current yaw
            sf::Vector3f forward = Camera::getForwardXZ(e); 
            sf::Vector3f right(-forward.z, 0.f, forward.x);

            sf::Vector3f moveVec(0.f, 0.f, 0.f);

            // Forward/Back (W/S)
            if (input.forward)  moveVec += forward;
            if (input.backward) moveVec -= forward;

            // Lateral (A/D only if Strafe modifier is active)
            if (!input.strafe) 
            {
                if (input.left)  moveVec -= right;
                if (input.right) moveVec += right;
            }
            // currentMoveSpeed is interpreted as units per second; convert to per-second velocity
            transform.velocity = Camera::normalize(moveVec) * currentMoveSpeed;

            if (moveVec.x != 0.f || moveVec.y != 0.f || moveVec.z != 0.f)
            {
                // Slow bob cadence by factor of ~3 to make motion gentler
                // Make walk cadence a bit quicker while leaving sprint unchanged
                float baseBobStep = playerState.sprinting ? 0.08f : 0.06f;
                float bobStep = baseBobStep * (1.0f / 3.0f);
                // previous code incremented per-frame; convert to per-second rate by scaling to 60fps baseline
                const float FRAME_BASE = 60.0f;
                playerState.bobAccumulator = std::fmod(playerState.bobAccumulator + bobStep * FRAME_BASE * dt, 1.f);
            }
        }

        // Apply shared physics movement for all entities
        transform.prevPos = transform.pos;
        transform.pos += transform.velocity * dt;
        
        // Grounding logic (The 172.0f "Floating" height)
        transform.pos.y = heightAt(transform.pos.x, transform.pos.z) + 172.0f;
    }
}

void Scene_IC_Camp::initializeTerrainLayers() {
    const float PI = 3.14159265f;
    const float layerRingRadius = 15000.0f;  // Distance of layer centers from origin

    for (int i = 0; i < 16; ++i) {
        // Arrange layers in a circle around the origin
        float angle = 2.0f * PI * (float)i / 16.0f;
        
        TerrainLayer& layer = m_terrainLayers[i];
        layer.center = sf::Vector2f(
            std::cos(angle) * layerRingRadius,
            std::sin(angle) * layerRingRadius
        );
        
        layer.radius = 5000.0f + (i % 4) * 1500.0f;  // Vary radius based on layer index
        layer.falloffWidth = 700.0f + (i % 3) * 800.0f;  // Vary falloff sharpness
        layer.topoHeight = 200.0f + (i % 8) * 50.0f;  // Vary height scale
    }
}

float Scene_IC_Camp::computeSceneMaxHeight() const {
    float maxHeight = 0.0f;
    for (const auto& layer : m_terrainLayers) {
        maxHeight = std::max(maxHeight, heightAt(layer.center.x, layer.center.y));
    }
    return std::max(1.0f, maxHeight * 1.25f);
}

float Scene_IC_Camp::evaluateLayerHeightAt(const TerrainLayer& layer, float x, float z) const {
    return layer.topoHeight;
}

float Scene_IC_Camp::getCameraHeightAboveGround(const sf::Vector3f& camPos) const {
    float groundHeight = heightAt(camPos.x, camPos.z);
    return camPos.y - groundHeight;
}

// Mask math in C++ matching GLSL `maskFromD` (user-specified formulas).
static float maskFromD_Cpp(float d, float rd, float falloff) {
    const float k = 1e-10f;
    float t = falloff;

    float s = 0.0f;
    float denom_s = d - k;
    if (std::fabs(denom_s) >= k) {
        s = (1.0f - std::fabs(d) / denom_s) * 0.5f;
    }

    float u = t - std::fabs(d - t);

    float g = 0.0f;
    float denom_g = std::fabs(u) - k;
    if (std::fabs(denom_g) >= k) {
        g = ((u / denom_g) + 1.0f) * 0.5f;
    }

    const float PI = 3.14159265358979323846f;
    float cosTerm = std::cos(PI * (d / (2.0f * t)));
    float b = g * ((cosTerm + 1.0f) * 0.5f);

    float m = s + b;
    if (m < 0.0f) m = 0.0f;
    if (m > 1.0f) m = 1.0f;

    return m;
}

uint32_t Scene_IC_Camp::computeActiveLayerMask(const sf::Vector3f& cameraPos) {
    // Compute which layers are within culling distance of the camera.
    // Layers beyond this distance won't significantly affect raymarching near the camera.
    // Higher culling distance = more stable mask (fewer frame-to-frame changes) = less flicker.
    const float LAYER_CULL_DIST = 150000.0f;  // Increased to reduce mask thrashing
    uint32_t mask = 0;
    
    for (int i = 0; i < 16; ++i) {
        float dx = cameraPos.x - m_terrainLayers[i].center.x;
        float dz = cameraPos.z - m_terrainLayers[i].center.y;
        float distSq = dx * dx + dz * dz;
        float cullRadiusSq = (LAYER_CULL_DIST + m_terrainLayers[i].radius) * 
                             (LAYER_CULL_DIST + m_terrainLayers[i].radius);
        if (distSq <= cullRadiusSq) {
            mask |= (1u << i);
        }
    }
    return mask;
}

void Scene_IC_Camp::uploadTerrainLayersToShader(sf::Shader& shader, const std::string& prefix) {
    for (int i = 0; i < 16; ++i) {
        const TerrainLayer& layer = m_terrainLayers[i];
        std::string idx = std::to_string(i);
        
        shader.setUniform(prefix + "_center[" + idx + "]", sf::Glsl::Vec2(layer.center.x, layer.center.y));
        shader.setUniform(prefix + "_radius[" + idx + "]", layer.radius);
        shader.setUniform(prefix + "_falloffWidth[" + idx + "]", layer.falloffWidth);
        shader.setUniform(prefix + "_topoHeight[" + idx + "]", layer.topoHeight);
    }
}

void Scene_IC_Camp::uploadActiveLayerMaskToShader(sf::Shader& shader, const std::string& prefix) {
    for (int i = 0; i < 16; ++i) {
        const float enabled = (m_activeLayerMask & (1u << i)) != 0 ? 1.0f : 0.0f;
        shader.setUniform(prefix + "[" + std::to_string(i) + "]", enabled);
    }
}

float Scene_IC_Camp::heightAt(float x, float z) const {
    float height = 0.0f;
    for (int i = 0; i < 16; ++i) {
        if ((m_activeLayerMask & (1u << i)) == 0) continue;

        const TerrainLayer& layer = m_terrainLayers[i];
        float dx = x - layer.center.x;
        float dz = z - layer.center.y;
        float dist = std::sqrt(dx * dx + dz * dz);
        float d = dist - layer.radius; // signed-distance-like

        float m = maskFromD_Cpp(d, layer.radius, layer.falloffWidth);
        if (m <= 0.0f) continue;

        float layerH = evaluateLayerHeightAt(layer, x, z);
        height += m * layerH;
    }
    return height;
}

/*
float contribution = 300.0(sin(xz.x/100.0)*sin(xz.z/100.0))
*/

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

void Scene_IC_Camp::sRender() {
    auto& window = m_game.window();
    auto& transform = m_camera->get<CTransform3D>();
    auto& cameraData = m_camera->get<CCamera>();
    sf::Vector2u winSize = window.getSize();
    auto inverseRotationMatrix = toGlslMat3(Camera::getInverseRotationMatrix(transform.pitch, transform.yaw, transform.roll));
    window.clear(sf::Color::Transparent);
    renderWorld(inverseRotationMatrix);
    runTopDownPass();
    runBakePass(inverseRotationMatrix);
    runFinalPass(inverseRotationMatrix);
    sf::Sprite finalSprite(m_renderTexture.getTexture());
    sf::Sprite backgroundSprite(m_skyTexture.getTexture());
    window.draw(backgroundSprite);
    window.draw(finalSprite);
    m_hud->render(m_game.window(), false);

    if (m_showTopDownViewer) {
        sf::Vector2u size = window.getSize();
        const float panelSize = 220.f;
        const float pad = 12.f;
        sf::Vector2f panelPos(pad, size.y - panelSize - pad);

        sf::RectangleShape panel(sf::Vector2f(panelSize, panelSize));
        panel.setPosition(panelPos);
        panel.setFillColor(sf::Color(14, 16, 18, 235));
        panel.setOutlineThickness(2.f);
        panel.setOutlineColor(sf::Color(230, 220, 200, 230));
        window.draw(panel);

        sf::Sprite viewer(m_topdownTexture.getTexture());
        const float sx = (panelSize - 8.f) / float(m_topdownTexture.getSize().x);
        const float sy = (panelSize - 8.f) / float(m_topdownTexture.getSize().y);
        viewer.setPosition(panelPos + sf::Vector2f(4.f, 4.f));
        viewer.setScale(sf::Vector2f(sx, sy));
        window.draw(viewer);
    }
}

void Scene_IC_Camp::sGUI()
{
    ImGui::Begin("Scene Properties##IC_Camp");

    ImGui::Text("FPS: %.1f", m_fps);

    // --- Camera controls ---
    if (ImGui::CollapsingHeader("Camera Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& transform = m_player->get<CTransform3D>();
        auto& cameraData = m_player->get<CCamera>();
        static float yaw = transform.yaw;
        static float pitch = transform.pitch;
        static float roll = transform.roll;
        static float pos[3] = { transform.pos.x, transform.pos.y, transform.pos.z };
        bool changed = false;
        changed |= ImGui::SliderFloat("Yaw (rad)", &yaw, -3.14f, 3.14f);
        changed |= ImGui::SliderFloat("Pitch (rad)", &pitch, -3.14f/2, 3.14f/2);
        changed |= ImGui::SliderFloat("Roll (rad)", &roll, -3.14f, 3.14f);
        changed |= ImGui::InputFloat3("Position (x, y, z)", pos, "%.2f");
        if (changed) {
            transform.yaw = yaw;
            transform.pitch = pitch;
            transform.roll = roll;
            transform.pos = sf::Vector3f(pos[0], pos[1], pos[2]);
        }
        ImGui::Text("Current: pos=(%.2f, %.2f, %.2f) pitch=%.2f yaw=%.2f roll=%.2f", transform.pos.x, transform.pos.y, transform.pos.z, transform.pitch, transform.yaw, transform.roll);
    }

    if (ImGui::BeginTabBar("MyTabBar"))
    {
        if (ImGui::BeginTabItem("Debug"))
        {
            ImGui::Checkbox("Draw Grid", &m_drawGrid);
            ImGui::Checkbox("Draw Textures", &m_drawTextures);
            ImGui::Checkbox("Draw Debug", &m_drawCollision);
            ImGui::Checkbox("Show Top-down Viewer", &m_showTopDownViewer);

            sf::Vector2i mousePos = sf::Mouse::getPosition(m_game.window());
            sf::Vector2f mouseScreen(float(mousePos.x), float(mousePos.y));

            sf::Vector3f worldPos = screenToWorld(mousePos);
            sf::Vector2i hexCoords = worldToHex(worldPos.x, worldPos.z);

            auto& playerTransform = m_player->get<CTransform3D>();
            sf::Vector3f rel = worldPos - playerTransform.pos;
            float dist = std::sqrt(rel.x*rel.x + rel.y*rel.y + rel.z*rel.z);

            ImGui::Text("Mouse screen: (%.1f, %.1f)", mouseScreen.x, mouseScreen.y);
            ImGui::Text("World pos: (%.1f, %.1f, %.1f)", worldPos.x, worldPos.y, worldPos.z);
            ImGui::Text("Hex coords: (%d, %d)", hexCoords.x, hexCoords.y);
            ImGui::Text("Distance: %.1f", dist);
            ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", 
                Camera::getForward(m_camera).x, 
                Camera::getForward(m_camera).y, 
                Camera::getForward(m_camera).z);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Animations"))
        {
            if (ImGui::CollapsingHeader("Animations", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                int count = 0;
                for (const auto& [name, anim] : Assets::Instance().getAnimations())
                {
                    count++;
                    ImGui::ImageButton(name.c_str(), anim.getSprite(), sf::Vector2f(32, 32));
                    if ((count % 6) != 0 && count != Assets::Instance().getAnimations().size()) { ImGui::SameLine(); }
                }
                ImGui::Unindent();
            }
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
            std::vector<std::shared_ptr<Entity>> entities;
            if (tags[currentTagIndex] == "ALL") entities = m_entities.getEntities();
            else entities = m_entities.getEntities(tags[currentTagIndex]);
            if (!entities.empty())
            {
                std::vector<std::string> entityLabels;
                for (auto e : entities)
                {
                    //Vec2f gridPos = midPixelToGrid(e->get<CTransform>().pos, e);
                    entityLabels.push_back(e->tag() + " " + std::to_string(e->id()) + " " + std::to_string(int(1)) + "," + std::to_string(int(0)));
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
                    ImGui::Text("ID: %d", int(entity->id()));
                    ImGui::Text("Tag: %s", entity->tag().c_str());
                    ImGui::Button("Destroy Entity");
                    if (ImGui::IsItemClicked())
                    {
                        if (entity->tag() != "player")
                        {
                            entity->destroy();
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
            changed |= ImGui::SliderFloat("Time of Day (hours)", &m_gameTimeOfDay, 0.0f, 24.0f);
            changed |= ImGui::SliderInt("Day of Year", &m_gameDayOfYear, 1, 365);
            changed |= ImGui::SliderFloat("Latitude", &m_latitude, -90.0f, 90.0f);
            if (changed) updateSunPosition();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Shader quality control
    ImGui::Begin("Rendering Quality");
    ImGui::SliderFloat("Render Quality", &m_shaderQuality, 0.05f, 1.0f, "%.2f");
    ImGui::Text("Lowering quality reduces GPU work (fewer march steps)");
    ImGui::SliderFloat("Step Size Scale", &m_stepSizeScale, 0.1f, 5.0f, "%.2f");
    ImGui::Text("Decrease to <1.0 for finer detail, increase for performance");
    ImGui::End();

    ImGui::End();
}

void Scene_IC_Camp::runBakePass(const sf::Glsl::Mat3& inverseRotationMatrix) {
    auto& transform = m_camera->get<CTransform3D>();
    auto& cameraData = m_camera->get<CCamera>();
    sf::Vector2u bakeSize = m_bakeTexture.getSize();

    // Compute which layers are close enough to affect raymarching
    m_activeLayerMask = computeActiveLayerMask(transform.pos);

    m_bakeShader.setUniform("viewportSize",  sf::Glsl::Vec2(bakeSize.x, bakeSize.y));
    m_bakeShader.setUniform("cameraPos",     sf::Glsl::Vec3(transform.pos.x, transform.pos.y, transform.pos.z));
    m_bakeShader.setUniform("invRotationMatrix", inverseRotationMatrix);
    m_bakeShader.setUniform("cameraYaw",     transform.yaw);
    m_bakeShader.setUniform("fovY",          cameraData.fovY);
    m_bakeShader.setUniform("aspectRatio",   cameraData.aspectRatio);
    m_bakeShader.setUniform("nearPlane",     cameraData.nearPlane);
    m_bakeShader.setUniform("farPlane",      cameraData.farPlane);
    m_bakeShader.setUniform("u_quality",     m_shaderQuality);
    m_bakeShader.setUniform("u_stepSizeScale", m_stepSizeScale);
    m_bakeShader.setUniform("topoTopdownTex", m_topdownTexture.getTexture());
    m_bakeShader.setUniform("topdownWorldMin", sf::Glsl::Vec2(m_topdownWorldMin.x, m_topdownWorldMin.y));
    m_bakeShader.setUniform("topdownWorldSize", sf::Glsl::Vec2(m_topdownWorldSize.x, m_topdownWorldSize.y));
    m_bakeShader.setUniform("topdownHeightMax", m_topdownMaxHeight);
    uploadTerrainLayersToShader(m_bakeShader, "layer");
    uploadActiveLayerMaskToShader(m_bakeShader, "u_activeLayerEnabled");

    sf::RectangleShape dummyRect(sf::Vector2f(bakeSize.x, bakeSize.y));
    m_bakeTexture.clear(sf::Color::Transparent);
    m_bakeTexture.draw(dummyRect, &m_bakeShader);
    m_bakeTexture.display();
}

void Scene_IC_Camp::runTopDownPass() {
    auto& transform = m_camera->get<CTransform3D>();
    auto& cameraData = m_camera->get<CCamera>();
    sf::Vector2u texSize = m_topdownTexture.getSize();
    sf::Vector2u winSize = m_game.window().getSize();

    auto makeFootprintCorner = [&](float sx, float sy) {
        float x_ndc = (sx / float(winSize.x)) * 2.0f - 1.0f;
        float y_ndc = 1.0f - (sy / float(winSize.y)) * 2.0f;
        float f = std::tan(cameraData.fovY * 0.5f);
        sf::Vector3f rayDir = Camera::rotateInverse(
            sf::Vector3f(x_ndc * f * cameraData.aspectRatio, y_ndc * f, -1.0f),
            transform.pitch, transform.yaw, transform.roll);
        rayDir = Camera::normalize(rayDir);

        sf::Vector3f world = transform.pos + rayDir * cameraData.farPlane;
        if (std::abs(rayDir.y) > 1e-5f) {
            float tGround = -transform.pos.y / rayDir.y;
            if (tGround > 0.0f) {
                world = transform.pos + rayDir * std::min(tGround, cameraData.farPlane);
            }
        }
        return sf::Glsl::Vec2(world.x, world.z);
    };

    sf::Glsl::Vec2 topLeft = makeFootprintCorner(0.f, 0.f);
    sf::Glsl::Vec2 topRight = makeFootprintCorner(float(winSize.x), 0.f);
    sf::Glsl::Vec2 bottomLeft = makeFootprintCorner(0.f, float(winSize.y));
    sf::Glsl::Vec2 bottomRight = makeFootprintCorner(float(winSize.x), float(winSize.y));

    float minX = std::min(std::min(topLeft.x, topRight.x), std::min(bottomLeft.x, bottomRight.x));
    float maxX = std::max(std::max(topLeft.x, topRight.x), std::max(bottomLeft.x, bottomRight.x));
    float minZ = std::min(std::min(topLeft.y, topRight.y), std::min(bottomLeft.y, bottomRight.y));
    float maxZ = std::max(std::max(topLeft.y, topRight.y), std::max(bottomLeft.y, bottomRight.y));
    m_topdownWorldMin = sf::Vector2f(minX, minZ);
    m_topdownWorldSize = sf::Vector2f(std::max(1.0f, maxX - minX), std::max(1.0f, maxZ - minZ));

    m_topdownShader.setUniform("viewportSize", sf::Glsl::Vec2(texSize.x, texSize.y));
    m_topdownShader.setUniform("cameraYaw", transform.yaw);
    m_topdownShader.setUniform("worldMin", sf::Glsl::Vec2(m_topdownWorldMin.x, m_topdownWorldMin.y));
    m_topdownShader.setUniform("worldSize", sf::Glsl::Vec2(m_topdownWorldSize.x, m_topdownWorldSize.y));
    m_topdownShader.setUniform("heightMax", m_topdownMaxHeight);
    uploadTerrainLayersToShader(m_topdownShader, "layer");
    for (int i = 0; i < 16; ++i) {
        m_topdownShader.setUniform("u_activeLayerEnabled[" + std::to_string(i) + "]", 1.0f);
    }

    sf::RectangleShape dummyRect(sf::Vector2f(texSize.x, texSize.y));
    m_topdownTexture.clear(sf::Color::Transparent);
    m_topdownTexture.setSmooth(true);
    m_topdownTexture.draw(dummyRect, &m_topdownShader);
    m_topdownTexture.display();
}

void Scene_IC_Camp::runFinalPass(const sf::Glsl::Mat3& inverseRotationMatrix) {
    auto& transform = m_camera->get<CTransform3D>();
    auto& cameraData = m_camera->get<CCamera>();
    sf::Vector2u winSize = m_game.window().getSize();
    sf::Vector3f worldPos = screenToWorld(sf::Mouse::getPosition(m_game.window()));
    sf::Vector2i hex = worldToHex(worldPos.x, worldPos.z);

    m_finalShader.setUniform("viewportSize",  sf::Glsl::Vec2(winSize.x, winSize.y));
    m_finalShader.setUniform("m_hexSize",     m_hexSize);
    m_finalShader.setUniform("cameraPos",     sf::Glsl::Vec3(transform.pos.x, transform.pos.y, transform.pos.z));
    m_finalShader.setUniform("farPlane",      cameraData.farPlane);
    m_finalShader.setUniform("nearPlane",     cameraData.nearPlane);
    m_finalShader.setUniform("fovY",          cameraData.fovY);
    m_finalShader.setUniform("aspectRatio",   cameraData.aspectRatio);
    m_finalShader.setUniform("invRotationMatrix", inverseRotationMatrix);
    m_finalShader.setUniform("camHeight",     getCameraHeightAboveGround(transform.pos));
    m_finalShader.setUniform("sunDir", m_sunDirection); 
    m_finalShader.setUniform("sunColor", m_sunColor);
    m_finalShader.setUniform("ambientStrength", 0.3f);
    m_finalShader.setUniform("baseColor", colorToShader(Theme::color("best-brown")));
    m_finalShader.setUniform("gridColor", colorToShader(m_gridColor));
        uploadTerrainLayersToShader(m_finalShader, "layer");
    m_finalShader.setUniform("topoTex", m_bakeTexture.getTexture());
    m_finalShader.setUniform("cursorMode", m_cursorMode);
    m_finalShader.setUniform("hoveredHex", sf::Glsl::Vec2((float)hex.x, (float)hex.y));
    const bool headlampOn = shouldHeadlightsBeOn();
    m_finalShader.setUniform("headlampOn", headlampOn);
    m_finalShader.setUniform("headlampIntensity", 4.f);
    m_finalShader.setUniform("headlampColor", colorToShader(sf::Color(255, 244, 214)));
    m_finalShader.setUniform("headlampRange", 15000.0f);
    uploadActiveLayerMaskToShader(m_finalShader, "u_activeLayerEnabled");
    m_renderTexture.clear(sf::Color::Transparent);
    sf::RectangleShape dummyRect(sf::Vector2f(winSize.x, winSize.y));
    m_renderTexture.draw(dummyRect, &m_finalShader);
    m_renderTexture.setSmooth(true);
    m_renderTexture.display();
}

void Scene_IC_Camp::renderWorld(const sf::Glsl::Mat3& rotationMatrix) {
    m_sky.setUniform("viewportSize",  sf::Glsl::Vec2(m_game.window().getSize().x, m_game.window().getSize().y));
    m_sky.setUniform("fovY",          m_camera->get<CCamera>().fovY);
    m_sky.setUniform("aspectRatio",   m_camera->get<CCamera>().aspectRatio);
    auto transform = m_camera->get<CTransform3D>();
    m_sky.setUniform("invRotationMatrix", rotationMatrix);
    m_sky.setUniform("sunDir", m_sunDirection);
    m_renderTexture.clear(sf::Color::Transparent);
    m_renderTexture.display();
    m_skyTexture.clear(sf::Color::Transparent);
    sf::Sprite groundSprite(m_renderTexture.getTexture());
    m_skyTexture.draw(groundSprite, &m_sky);
    m_skyTexture.setSmooth(true);
    m_skyTexture.display();
}

void Scene_IC_Camp::updateSunPosition()
{
    float hourFraction = m_gameTimeOfDay / 24.0f;

    // Solar declination (approximate)
    float declination = 23.45f * std::sin((360.0f / 365.0f) * (m_gameDayOfYear - 81.0f) * 3.14159265f / 180.0f);

    float latRad = m_latitude * 3.14159265f / 180.0f;
    float decRad = declination * 3.14159265f / 180.0f;

    // Hour angle in radians (-180° to +180°)
    float hourAngleDeg = (hourFraction - 0.5f) * 360.0f;
    float haRad = hourAngleDeg * 3.14159265f / 180.0f;

    // Solar elevation
    float sinElevation = std::sin(latRad) * std::sin(decRad) + 
                         std::cos(latRad) * std::cos(decRad) * std::cos(haRad);
    
    sinElevation = std::clamp(sinElevation, -1.0f, 1.0f);   // safety
    float elevationRad = std::asin(sinElevation);
    float elevationDeg = elevationRad * 180.0f / 3.14159265f;

    // === Solar Azimuth (more robust calculation) ===
    float azimuthRad;

    if (std::abs(std::cos(elevationRad)) < 0.0001f) {
        // Sun nearly directly overhead or below horizon — azimuth is ambiguous
        azimuthRad = 0.0f;   // or keep previous value
    } else {
        float sinAz = std::sin(haRad) * std::cos(decRad) / std::cos(elevationRad);
        float cosAz = (std::sin(decRad) - std::sin(latRad) * std::sin(elevationRad)) / 
                      (std::cos(latRad) * std::cos(elevationRad));

        // Use atan2 for proper quadrant
        azimuthRad = std::atan2(sinAz, cosAz);
    }

    // Convert to 3D direction vector (Y up)
    m_sunDirection = sf::Glsl::Vec3(
        std::cos(elevationRad) * std::sin(azimuthRad),   // X (East-West)
        std::sin(elevationRad),                          // Y (Up)
        std::cos(elevationRad) * std::cos(azimuthRad)    // Z (North-South)
    );

    // === Sun Color & Intensity ===
    float sunHeightFactor = std::clamp((elevationDeg + 12.0f) / 90.0f, 0.0f, 1.0f);

    m_sunIntensity = sunHeightFactor * 1.25f + 0.25f;

    // Golden hour / twilight warmth
    float warmth = 1.0f - sunHeightFactor * 0.75f;           // stronger warmth near horizon

    m_sunColor = sf::Glsl::Vec4(
        1.00f,
        0.90f + warmth * 0.10f,
        0.65f + warmth * 0.30f,
        m_sunIntensity
    );
}

void Scene_IC_Camp::captureBake() {
    // Capture the current bake texture for use in the HUD or other UI elements
    m_currentBakeImage = m_bakeTexture.getTexture().copyToImage();
}

sf::Vector3f Scene_IC_Camp::screenToWorld(sf::Vector2i position) const {
    if (!m_cursorMode) {
        return { 0.f, 0.f, 0.f };
    }

    auto& cam = m_camera->get<CTransform3D>();
    auto& camConfig = m_cameraConfig;

    if (position.x < 0 || position.y < 0 ||
        position.x >= (int)camConfig.VIEWPORT_WIDTH || position.y >= (int)camConfig.VIEWPORT_HEIGHT) {
        return { 0.f, 0.f, 0.f };
    }

    float aspectRatio = float(camConfig.VIEWPORT_WIDTH) / camConfig.VIEWPORT_HEIGHT;

    // Reconstruct ray — must match bake shader exactly
    float x_ndc = (position.x / float(camConfig.VIEWPORT_WIDTH)) * 2.0f - 1.0f;
    float y_ndc = 1.0f - (position.y / float(camConfig.VIEWPORT_HEIGHT)) * 2.0f;
    float f = std::tan(camConfig.FOVY * 0.5f);

    sf::Vector3f rayDir = Camera::rotateInverse(sf::Vector3f(x_ndc * f * aspectRatio, y_ndc * f, -1.0f), cam.pitch, cam.yaw, cam.roll);
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