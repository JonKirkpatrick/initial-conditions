
#include "Scene_IC_Camp.h"
#include "GameEngine.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <cmath>
#include "imgui.h"
#include "imgui-SFML.h"
#include "Camera.h"
#include <random>

// This might need to move somewhere else.  It's just to flatten a 3x3 matrix into column-major order for uploading to the shader, and I need it in multiple places now.
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

Scene_IC_Camp::Scene_IC_Camp(GameEngine& game, const std::string& levelPath)
    : Scene(game)
    , m_levelPath(levelPath)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    sf::Vector2u windowSize = game.window().getSize();
    m_renderTexture = sf::RenderTexture({windowSize.x, windowSize.y});
    m_skyTexture = sf::RenderTexture({windowSize.x, windowSize.y});
    sf::Vector2u bakeSize(
        std::max(1u, windowSize.x / 1),
        std::max(1u, windowSize.y / 1)
    );
    m_bakeTexture = sf::RenderTexture({bakeSize.x, bakeSize.y});
    m_bakeTexture.setSmooth(false);
    m_topdownTexture = sf::RenderTexture({m_topdownTextureSize, m_topdownTextureSize});
    m_topdownTexture.setSmooth(false);
    m_minimapTexture = sf::RenderTexture({m_minimapTextureSize, m_minimapTextureSize});
    m_gridColor = Theme::color("cerulean");
    m_cameraConfig.VIEWPORT_WIDTH = windowSize.x;
    m_cameraConfig.VIEWPORT_HEIGHT = windowSize.y;
    loadLevel(m_levelPath);
    spawnCamera();
    spawnPlayer();
    spawnDebugOrbs(8000);

    // Sync legacy tuple components into SoA before any code reads from the new storage.
    m_entityManager.update();

    Topography::setWarpParameters(m_warpScale, m_warpStrength);
    m_topdownMaxHeight = computeSceneMaxHeight();
    buildHud();
    updateHUDData();
    updateSunPosition();
    m_game.setMouseCaptured(true);
    m_cursorMode = false;
}

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
    forward.y = 0.f;
    forward = Camera::normalize(forward);
    sf::Vector3f right(forward.z, 0.f, -forward.x);

    float horizontalSpeed = std::sqrt(
        playerTransform.velocity.x * playerTransform.velocity.x +
        playerTransform.velocity.z * playerTransform.velocity.z
    );

    // Speed as a fraction of base move speed, clamped to sprint ceiling.
    // Slope and terrain are already baked into horizontalSpeed, so no
    // further compensation needed here.
    float speedFraction = std::clamp(horizontalSpeed / std::max(m_playerConfig.MOVE_SPEED, 0.0001f), 0.0f, 3.0f);

    // moveFactor gates the bob entirely when nearly still (0..1 over first unit of speed)
    float moveFactor = std::clamp(speedFraction, 0.0f, 1.0f);

    float phase = playerBob.accumulator * 6.2831853f;

    // === Bob Parameters ===
    float baseFrequency = 1.0f;

    // Amplitudes interpolate continuously with speed.
    // Sprint end (speedFraction = 3) is tighter/smaller — you're more rigid at a run.
    // Walk end (speedFraction = 1) is the most pronounced swing.
    // Uphill crawl (speedFraction < 1) tapers down toward zero via moveFactor.
    float t = std::clamp((speedFraction - 1.0f) / 2.0f, 0.0f, 1.0f); // 0 = walk, 1 = full sprint
    float lateralAmplitude = 5.5f + (3.8f - 5.5f) * t;
    float verticalAmplitude = 6.2f + (4.8f - 6.2f) * t;

    // Crouch reduces amplitude as a postural state regardless of speed
    if (playerPhysics.isCrouching)
    {
        lateralAmplitude  *= (1.0f - m_crouchFactor * 0.45f);
        verticalAmplitude *= (1.0f - m_crouchFactor * 0.55f);
    }

    lateralAmplitude *= 0.85f;

    // Use same base frequency, with mild harmonic on vertical
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

void Scene_IC_Camp::updateHUDData()
{
    // Use a constant for clarity
    const float RAD_TO_DEG = 180.0f / 3.14159265f;

    auto& playerTransform = m_entityManager.getTransform(m_player);
    sf::Vector3f currentLocation = playerTransform.pos;
    sf::Vector3f forward = forwardFromTransform(playerTransform);
    forward.y = 0.f;
    forward = Camera::normalize(forward);

    float currentHeading = -std::atan2(forward.x, forward.z) * RAD_TO_DEG;

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

HUD* Scene_IC_Camp::getHUD() const
{
    return m_hud.get();
}

void Scene_IC_Camp::loadLevel(const std::string& filename)
{
    m_entityManager = EntityManager();
    m_terrainLayers = {};

    std::ifstream file(filename);
    std::string str;
    int terrainLayerIndex = 0;
    while (file >> str)
    {
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
        if (str == "TerrainLayer")
        {
            float centerX = 0.f;
            float centerY = 0.f;
            float radius = 0.f;
            float falloffWidth = 0.f;
            float topoHeight = 0.f;

            file >> centerX
                 >> centerY
                 >> radius
                 >> falloffWidth
                 >> topoHeight;

            if (terrainLayerIndex < static_cast<int>(m_terrainLayers.size()))
            {
                TerrainLayer& layer = m_terrainLayers[terrainLayerIndex++];
                layer.center = sf::Vector2f(centerX, centerY);
                layer.radius = radius;
                layer.falloffWidth = falloffWidth;
                layer.topoHeight = topoHeight;
            }
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
    
    m_entityManager.addPlayer(m_player, CPlayer());
    m_entityManager.addTransform(m_player, CTransform3D(
        sf::Vector3f(m_playerConfig.POSITION_X, 
                     Scene_IC_Camp::heightAt(m_playerConfig.POSITION_X, m_playerConfig.POSITION_Z), 
                     m_playerConfig.POSITION_Z)
        ));
    
    m_entityManager.addInput(m_player, CInput());
    m_entityManager.addPhysics(m_player, CPhysics());
    m_entityManager.addBob(m_player, CBob(1.0f, 6.0f, 5.5f));   // rate, vertical mag, lateral mag

    // Optional: set initial values
    auto& phys = m_entityManager.getPhysics(m_player);
    phys.onGround = true;
}

void Scene_IC_Camp::spawnOrb(int hexQ, int hexR, const sf::Color& color, float radius, float bobRate, float bobMagnitude)
{
    auto orb = m_entityManager.addEntity("orb");
    sf::Vector2f worldPos = hexToWorld(hexQ, hexR);
    
    m_entityManager.addTransform(orb, CTransform3D(
        sf::Vector3f(worldPos.x, heightAt(worldPos.x, worldPos.y) + 100.0f, worldPos.y)
        ));
    
    m_entityManager.addOrb(orb, COrb(color, radius, 100.0f));
    m_entityManager.addBob(orb, CBob(bobRate, bobMagnitude, 0.0f));   // orbs only bob vertically
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

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> hexQDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> hexRDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> radiusDist(20.0f, 80.0f);
    std::uniform_real_distribution<float> bobRateDist(0.5f, 3.0f);
    std::uniform_real_distribution<float> bobMagDist(4.0f, 12.0f);

    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 16);
        }
    };
    std::unordered_set<std::pair<int,int>, PairHash> usedCoords;

    int spawned = 0;
    int maxAttempts = count * 10; // prevent infinite loop if grid is saturated
    int attempts = 0;

    while (spawned < count && attempts < maxAttempts)
    {
        ++attempts;
        int hexQ = static_cast<int>(hexQDist(rng));
        int hexR = static_cast<int>(hexRDist(rng));

        if (!usedCoords.insert({hexQ, hexR}).second)
            continue; // already taken, retry

        const sf::Color& color = palette[spawned % palette.size()];
        float radius  = radiusDist(rng);
        float bobRate = bobRateDist(rng);
        float bobMag  = bobMagDist(rng);
        spawnOrb(hexQ, hexR, color, radius, bobRate, bobMag);
        ++spawned;
    }
}

void Scene_IC_Camp::updateShadowOrbs()
{
    m_shadowOrbList.clear();

    auto& transform  = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);

    constexpr int   MAX_SHADOW_ORBS    = 64;
    constexpr float SHADOW_CUTOFF_DIST = 5000.0f; // 50 metres in cm

    struct CandidateOrb {
        sf::Vector3f worldPos;
        float        radius;
        float        depth;
    };

    std::vector<CandidateOrb> candidates;
    candidates.reserve(m_entityManager.getEntities("orb").size());

    for (auto orb : m_entityManager.getEntities("orb"))
    {
        if (!m_entityManager.hasOrb(orb) || !m_entityManager.hasTransform(orb) || !m_entityManager.hasBob(orb)) continue;

        auto& orbTransform = m_entityManager.getTransform(orb);
        auto& orbData      = m_entityManager.getOrb(orb);
        auto& orbBob       = m_entityManager.getBob(orb);

        float currentPhase = std::fmod(orbBob.accumulator, 1.0f);
        float bobOffset = std::sin(currentPhase * 6.2831853f) * orbBob.magnitude;

        sf::Vector3f shadowWorldPos = orbTransform.pos;
        shadowWorldPos.y = heightAt(shadowWorldPos.x, shadowWorldPos.z) 
                           + orbData.heightAboveGround + bobOffset;

        sf::Vector3f relative    = shadowWorldPos - transform.pos;
        sf::Vector3f cameraSpace = Camera::worldToCamera(relative, transform.pitch, transform.yaw, transform.roll);

        if (cameraSpace.z >= -cameraData.nearPlane) continue;

        float depth = std::sqrt(relative.x * relative.x + 
                        relative.y * relative.y + 
                        relative.z * relative.z);
        if (depth > SHADOW_CUTOFF_DIST) continue;

        candidates.push_back({ shadowWorldPos, orbData.radius, depth });
    }

    std::sort(candidates.begin(), candidates.end(), [](const CandidateOrb& a, const CandidateOrb& b) {
        return a.depth < b.depth;
    });

    int count = std::min((int)candidates.size(), MAX_SHADOW_ORBS);
    m_shadowOrbList.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        m_shadowOrbList.push_back({ candidates[i].worldPos, candidates[i].radius });
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

void Scene_IC_Camp::spawnCamera()
{
    m_camera = m_entityManager.addEntity("camera");
    m_entityManager.addCamera(m_camera, CCamera(
        m_cameraConfig.FOVY,
        float(m_cameraConfig.VIEWPORT_WIDTH)/m_cameraConfig.VIEWPORT_HEIGHT,
        m_cameraConfig.NEAR_PLANE,
        m_cameraConfig.FAR_PLANE,
        sf::Vector2u(m_cameraConfig.VIEWPORT_WIDTH, m_cameraConfig.VIEWPORT_HEIGHT)
        ));
    m_entityManager.addTransform(m_camera, CTransform3D(
        sf::Vector3f(m_cameraConfig.POSITION_X, m_cameraConfig.POSITION_Y, m_cameraConfig.POSITION_Z),
        sf::Vector3f(0.f, 0.f, 0.f),
        sf::Vector3f(1.f, 1.f, 1.f),
        m_cameraConfig.PITCH,
        m_cameraConfig.YAW,
        m_cameraConfig.ROLL
    ));
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
    const float realSecondsPerGameHour = 4.0f;
    m_gameTimeOfDay += dt * (1.0f / realSecondsPerGameHour);

    if (m_gameTimeOfDay >= 24.0f)
    {
        m_gameTimeOfDay -= 24.0f;
        m_gameDayOfYear += 1;
        if (m_gameDayOfYear > 365)
            m_gameDayOfYear = 1;
    }
    sMovement(dt);
    updateHUDData();
    updateSunPosition();
    updateCamera(dt);
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
            if (m_cursorMode) {
                captureBake();
            }
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

        // Kinematic movement for entities without physics (orbs, etc.)
        if (!m_entityManager.hasPhysics(e))
        {
            t.pos += t.velocity * dt;
        }

        // Note: physics integration moved to SoA-accelerated loop below

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

    auto& t       = m_entityManager.getTransform(e);
    auto& input   = m_entityManager.getInput(e);
    auto& phys    = m_entityManager.getPhysics(e);
    auto& bob     = m_entityManager.getBob(e);

    // Locomotion state
    phys.isCrouching = input.crouch;
    phys.isSprinting = input.sprint && !phys.isCrouching;

    float moveSpeed = m_playerConfig.MOVE_SPEED;
    if (phys.isSprinting)        moveSpeed *= 3.0f;
    else if (phys.isCrouching)   moveSpeed *= 0.6f;

    float sampleDist = 10.0f; // cm — tune to your terrain scale
    auto& playerTransform = m_entityManager.getTransform(e);
    sf::Vector3f fwd = forwardFromTransform(playerTransform);
    fwd.y = 0.f;
    fwd = Camera::normalize(fwd); // already normalised XZ

    float hAhead  = heightAt(t.pos.x + fwd.x * sampleDist, t.pos.z + fwd.z * sampleDist);
    float hBehind = heightAt(t.pos.x - fwd.x * sampleDist, t.pos.z - fwd.z * sampleDist);

    float slope = (hAhead - hBehind) / (2.0f * sampleDist); // rise over run

    float slopeBoost;
    if (slope >= 0.0f)
    {
        // Uphill — simple penalty as before
        slopeBoost = -std::clamp(slope * 2.0f, 0.0f, 0.4f);
    }
    else
    {
        // Downhill — peaks around 15% grade then falls off
        float grade = -slope; // positive for downhill
        float peak  = 0.15f;  // grade at which you're fastest
        float boost = (grade / peak) * std::exp(1.0f - grade / peak) * 0.25f;
        slopeBoost  = std::clamp(boost, -0.5f, 0.25f);
        // negative slopeBoost = penalty (too steep), positive = speed gain (gentle decline)
    }

    float slopeScale = 1.0f + slopeBoost;

    moveSpeed *= slopeScale;

    // === Rotation ===
    // Apply mouse look: invert signs so moving mouse right increases yaw and
    // moving mouse up increases pitch (match user expectations / conventions)
    t.yaw   += input.mouseDelta.x * 0.002f;
    t.pitch += input.mouseDelta.y * 0.002f;
    input.mouseDelta = { 0.f, 0.f };

    if (input.strafe)
    {
        if (input.left)  t.yaw += m_playerConfig.ROTATION_SPEED * dt;
        if (input.right) t.yaw -= m_playerConfig.ROTATION_SPEED * dt;
    }

    t.pitch = std::clamp(t.pitch, -1.57f, 1.57f);

    // === Movement Direction ===
    sf::Vector3f forward = forwardFromTransform(t);
    forward.y = 0.f;
    forward = Camera::normalize(forward);
    sf::Vector3f right   = {-forward.z, 0.0f, forward.x};

    sf::Vector3f moveDir(0.f, 0.f, 0.f);
    if (input.forward)  moveDir += forward;
    if (input.backward) moveDir -= forward;
    if (!input.strafe)
    {
        if (input.left)  moveDir -= right;
        if (input.right) moveDir += right;
    }

    // === Kinematic Horizontal Movement with Air Control ===
    float currentSpeed = std::sqrt(t.velocity.x*t.velocity.x + t.velocity.z*t.velocity.z);

    if (moveDir.x != 0.0f || moveDir.z != 0.0f)
    {
        sf::Vector3f desired = Camera::normalize(moveDir) * moveSpeed;

        if (phys.onGround)
        {
            // Full ground control (snappy)
            t.velocity.x = desired.x;
            t.velocity.z = desired.z;
        }
        else
        {
            // Limited air control
            float airControl = 0.25f;                    // ← tune this (0.2 ~ 0.4 feels good)
            t.velocity.x = t.velocity.x * (1.0f - airControl) + desired.x * airControl;
            t.velocity.z = t.velocity.z * (1.0f - airControl) + desired.z * airControl;
        }
    }
    else if (phys.onGround)
    {
        // Stop on ground when no input
        t.velocity.x = 0.0f;
        t.velocity.z = 0.0f;
    }
    // === Jumping ===
    if (input.jump && phys.onGround && !phys.isCrouching)
    {
        t.velocity.y = phys.jumpSpeed;
        phys.onGround = false;
    }

    // === Bobbing ===
    float horizSpeed = std::sqrt(t.velocity.x * t.velocity.x + t.velocity.z * t.velocity.z);
    updateBob(e, dt, horizSpeed);   // uses complex player path

    // Footsteps
    auto& bobComp = m_entityManager.getBob(e);
    if (horizSpeed > 1.0f && phys.onGround)
    {
        float currentPhase = bobComp.accumulator;

        // Trigger footsteps at roughly 0.0 and 0.5 in the cycle
        bool shouldStep = false;
        bool isLeft = true;
        bool isSprinting = phys.isSprinting;
        bool isCrouching = phys.isCrouching;

        // ... rest of your existing footstep logic unchanged ...
        if ((m_lastStepPhase > 0.8f && currentPhase < 0.2f) ||
            (m_lastStepPhase < 0.2f && currentPhase > 0.8f))
        {
            shouldStep = true;
            isLeft = true;
        }
        else if ((m_lastStepPhase < 0.45f && currentPhase >= 0.45f) ||
                 (m_lastStepPhase > 0.55f && currentPhase <= 0.55f))
        {
            shouldStep = true;
            isLeft = false;
        }

        if (shouldStep)
        {
            const std::string& soundName = isLeft ? "FootLeft" : "FootRight";
            float volume = isSprinting ? 75.f : (isCrouching ? 30.f : 45.f);
            AudioManager::Instance().sfx.playSound(Assets::Instance().getSound(soundName), volume);
        }

        m_lastStepPhase = currentPhase;
    }
}

// Unified bob update for any entity with CBob
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

void Scene_IC_Camp::updateOrbBobbing(SoAEntityHandle e, float dt)
{
    if (m_entityManager.getTag(e) != "orb") return;
    auto& t = m_entityManager.getTransform(e);
    auto& orb = m_entityManager.getOrb(e);

    updateBob(e, dt);  // uses simple path

    auto& bob = m_entityManager.getBob(e);
    float bobOffset = std::sin(bob.accumulator * 6.2831853f) * bob.magnitude;

    float groundY = heightAt(t.pos.x, t.pos.z);
    t.pos.y = groundY + orb.heightAboveGround + bobOffset;

    t.velocity.y = 0.0f;
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

// This picks out one of the 16 terrain layers to render a sedamentary stripe pattern on.
// It chooses the layer with the highest ratio of topoHeight to falloffWidth, which tends to produce the most visually striking stripes.
int Scene_IC_Camp::selectStripeLayerIndex() const {
    int bestIndex = -1;
    float bestScore = -1.0f;

    for (int i = 0; i < 16; ++i) {
        if ((m_activeLayerMask & (1u << i)) == 0) continue;

        const TerrainLayer& layer = m_terrainLayers[i];
        if (layer.falloffWidth <= 1e-6f) continue;

        float score = layer.topoHeight / layer.falloffWidth;
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return bestIndex;
}

// This is used already, but will become a far more central function as I start building out the orb system.
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
    auto& transform = m_entityManager.getTransform(m_camera);
    auto inverseRotationMatrix = toGlslMat3(Camera::getInverseRotationMatrix(transform.pitch, transform.yaw, transform.roll));
    window.clear(sf::Color::Transparent);
    runTopDownPass();
    updateShadowOrbs();
    uploadShadowOrbsToShader(m_finalShader);


    if (m_useDepthStepDebug) {
        runDepthStepPass(inverseRotationMatrix);
        sf::Sprite debugSprite(m_renderTexture.getTexture());
        window.draw(debugSprite);
    } else {
        renderSky(inverseRotationMatrix);
        runBakePass(inverseRotationMatrix);
        runFinalPass(inverseRotationMatrix);
        sf::Sprite finalSprite(m_renderTexture.getTexture());
        sf::Sprite backgroundSprite(m_skyTexture.getTexture());
        window.draw(backgroundSprite);
        window.draw(finalSprite);
        renderOrbs();
    }
    m_hud->render(window, false);

    if (m_showTopDownViewer) { renderTopDownViewer(window); }
}

//==== ImGui ====

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
            ImGui::Checkbox("Show Top-down Viewer", &m_showTopDownViewer);

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
    ImGui::Checkbox("Depth/Step Debug Bypass", &m_useDepthStepDebug);
    ImGui::Text("Bypasses the final composite and shows step cost directly.");
    ImGui::SliderFloat("Render Quality", &m_shaderQuality, 0.05f, 1.0f, "%.2f");
    ImGui::Text("Lowering quality reduces GPU work (fewer march steps)");
    ImGui::SliderFloat("Step Size Scale", &m_stepSizeScale, 0.1f, 5.0f, "%.2f");
    ImGui::Text("Decrease to <1.0 for finer detail, increase for performance");
    ImGui::SliderFloat("Step Contribution Scale", &m_stepContributionScale, 0.1f, 8.0f, "%.2f");
    ImGui::Text("Scales the step-count channel only; does not change marching behavior");
    ImGui::SliderFloat("Step Count Normalization Max", &m_stepCountNormalizationMax, 50.0f, 500.0f, "%.0f");
    ImGui::Text("Fixed divisor for step count; adjust to baseline, then change threshold");
    ImGui::SliderFloat("Heightmap Transition Threshold", &m_heightmapTransitionThreshold, 10.0f, 500.0f, "%.1f");
    ImGui::Text("Distance above cached heightmap before switching to raymarching");
    ImGui::Separator();
        bool warpChanged = false;
        warpChanged |= ImGui::SliderFloat("Warp Scale", &m_warpScale, 0.00002f, 0.00025f, "%.5f");
        warpChanged |= ImGui::SliderFloat("Warp Strength", &m_warpStrength, 0.0f, 2500.0f, "%.0f");
        if (warpChanged) {
            Topography::setWarpParameters(m_warpScale, m_warpStrength);
        }
        ImGui::Text("Warp is shared by C++ terrain queries and all terrain shaders");
    ImGui::End();

    ImGui::End();
}

// ==== Terrain and Shader Logic ====
// Our terrain is composed of 16 individual "regions".  The following three methods are concerned with provided the shaders with
// the necessary parameters to evaluate the contribution of each layer to the final terrain height at a given point.
void Scene_IC_Camp::uploadTerrainLayersToShader(sf::Shader& shader, const std::string& prefix) {
    uploadWarpParametersToShader(shader);
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

void Scene_IC_Camp::uploadWarpParametersToShader(sf::Shader& shader) const {
    shader.setUniform("u_warpScale", m_warpScale);
    shader.setUniform("u_warpStrength", m_warpStrength);
}

// This is the heart of the rendering system: it runs a fullscreen shader that raymarches the terrain to produce a depth value for each pixel, 
// which is stored in m_bakeTexture.  This is then sampled in the final pass to composite the terrain with the sky and orbs.
void Scene_IC_Camp::runBakePass(const sf::Glsl::Mat3& inverseRotationMatrix) {
    auto& transform = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    sf::Vector2u bakeSize = m_bakeTexture.getSize();

    // Compute which layers are close enough to affect raymarching
    m_activeLayerMask = Topography::computeActiveLayerMask(transform.pos, m_terrainLayers);

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
    m_bakeShader.setUniform("u_heightmapTransitionThreshold", m_heightmapTransitionThreshold);    
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

// This is a debug pass that is intended to visualize the cost of raymarching steps in the bake shader.
// It operates in a similar fashion to the bake shader, but reserves the red channel to encode the number of
// steps taken instead of providing 24 bit precision for depth.
void Scene_IC_Camp::runDepthStepPass(const sf::Glsl::Mat3& inverseRotationMatrix) {
    auto& transform = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    sf::Vector2u outputSize = m_renderTexture.getSize();

    m_depthStepShader.setUniform("viewportSize",  sf::Glsl::Vec2(outputSize.x, outputSize.y));
    m_depthStepShader.setUniform("cameraPos",     sf::Glsl::Vec3(transform.pos.x, transform.pos.y, transform.pos.z));
    m_depthStepShader.setUniform("invRotationMatrix", inverseRotationMatrix);
    m_depthStepShader.setUniform("cameraYaw",     transform.yaw);
    m_depthStepShader.setUniform("fovY",          cameraData.fovY);
    m_depthStepShader.setUniform("aspectRatio",   cameraData.aspectRatio);
    m_depthStepShader.setUniform("nearPlane",     cameraData.nearPlane);
    m_depthStepShader.setUniform("farPlane",      cameraData.farPlane);
    m_depthStepShader.setUniform("u_quality",     m_shaderQuality);
    m_depthStepShader.setUniform("u_stepSizeScale", m_stepSizeScale);
    m_depthStepShader.setUniform("u_stepContributionScale", m_stepContributionScale);
    m_depthStepShader.setUniform("u_stepCountNormalizationMax", m_stepCountNormalizationMax);
    m_depthStepShader.setUniform("u_heightmapTransitionThreshold", m_heightmapTransitionThreshold);
    m_depthStepShader.setUniform("topoTopdownTex", m_topdownTexture.getTexture());
    m_depthStepShader.setUniform("topdownWorldMin", sf::Glsl::Vec2(m_topdownWorldMin.x, m_topdownWorldMin.y));
    m_depthStepShader.setUniform("topdownWorldSize", sf::Glsl::Vec2(m_topdownWorldSize.x, m_topdownWorldSize.y));
    m_depthStepShader.setUniform("topdownHeightMax", m_topdownMaxHeight);
    uploadTerrainLayersToShader(m_depthStepShader, "layer");
    uploadActiveLayerMaskToShader(m_depthStepShader, "u_activeLayerEnabled");

    sf::RectangleShape dummyRect(sf::Vector2f(outputSize.x, outputSize.y));
    m_renderTexture.clear(sf::Color::Transparent);
    m_renderTexture.draw(dummyRect, &m_depthStepShader);
    m_renderTexture.setSmooth(true);
    m_renderTexture.display();
}

// This may still benefit from a bit of refinement, but it's intended to aid the bake shader in its job
// raymarching.  By rendering a top down view bounded by the camera's frustum and farplane, the bake
// shader can sample this instead of expensive raymarching steps until the ray is close enough to the ground
// to warrant the full reaymarching.
void Scene_IC_Camp::runTopDownPass() {
    auto& transform = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    sf::Vector2u texSize = m_topdownTexture.getSize();
    sf::Vector2u winSize = m_game.window().getSize();
    m_topdownMaxHeight = computeSceneMaxHeight();

    auto makeFootprintCorner = [&](float sx, float sy) {
        float x_ndc = (sx / float(winSize.x)) * 2.0f - 1.0f;
        float y_ndc = 1.0f - (sy / float(winSize.y)) * 2.0f;
        float f = std::tan(cameraData.fovY * 0.5f);
        sf::Vector3f rayDir = Camera::cameraToWorld(
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

// This pass uses the baked depthmap alone to produce a final shaded image.
void Scene_IC_Camp::runFinalPass(const sf::Glsl::Mat3& inverseRotationMatrix) {
    auto& transform = m_entityManager.getTransform(m_camera);
    auto& cameraData = m_entityManager.getCamera(m_camera);
    sf::Vector2u winSize = m_game.window().getSize();
    sf::Vector3f worldPos = screenToWorld(sf::Mouse::getPosition(m_game.window()));
    sf::Vector2i hex = worldToHex(worldPos.x, worldPos.z);
    m_stripeLayerIndex = selectStripeLayerIndex();

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
    m_finalShader.setUniform("u_stripeLayerIndex", m_stripeLayerIndex);
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

// Draws the sky background.
void Scene_IC_Camp::renderSky(const sf::Glsl::Mat3& rotationMatrix) {
    auto& cameraData = m_entityManager.getCamera(m_camera);
    m_sky.setUniform("viewportSize",  sf::Glsl::Vec2(m_game.window().getSize().x, m_game.window().getSize().y));
    m_sky.setUniform("fovY",          cameraData.fovY);
    m_sky.setUniform("aspectRatio",   cameraData.aspectRatio);
    auto transform = m_entityManager.getTransform(m_camera);
    m_sky.setUniform("invRotationMatrix", rotationMatrix);
    m_sky.setUniform("sunDir", m_sunDirection);
    m_renderTexture.clear(sf::Color::Transparent);
    m_renderTexture.display();
    m_skyTexture.clear(sf::Color::Transparent);
    sf::Sprite skySprite(m_renderTexture.getTexture());
    m_skyTexture.draw(skySprite, &m_sky);
    m_skyTexture.setSmooth(true);
    m_skyTexture.display();
}

// This is a diagnostic tool.  It draws a small top-down view based on the texture produced
// for consumption by the bake pass.  It is useful for debugging, but is generally not intended
// to be displayed.
void Scene_IC_Camp::renderTopDownViewer(sf::RenderWindow& window) {
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

// Helper to upload one batch to the shader
void Scene_IC_Camp::uploadOrbBatchToShader(sf::Shader& shader, const OrbBatch& batch,
                                           const sf::Vector3f& sunDirView)
{
    int size = static_cast<int>(batch.centersView.size());
    shader.setUniform("u_batchSize", size);

    if (size > 0)
    {
        shader.setUniformArray("u_orbCenterView", batch.centersView.data(), size);
        shader.setUniformArray("u_orbColor",      batch.colors.data(), size);
        shader.setUniformArray("u_orbDepthNorm",  batch.depthNorms.data(), size);
        shader.setUniformArray("u_quadOrigin",    batch.quadOrigins.data(), size);
        shader.setUniformArray("u_texSize",       batch.texSizes.data(), size);
    }

    shader.setUniform("sunDir", sf::Glsl::Vec3(sunDirView.x, sunDirView.y, sunDirView.z));
    shader.setUniform("sunColor", m_sunColor);
    shader.setUniform("u_bakeTex", m_bakeTexture.getTexture());
    shader.setUniform("u_viewportSize", sf::Glsl::Vec2(
        static_cast<float>(m_bakeTexture.getSize().x),
        static_cast<float>(m_bakeTexture.getSize().y)));

    shader.setUniform("headlampEnabled", shouldHeadlightsBeOn() ? 1.0f : 0.0f);
    shader.setUniform("headlampIntensity", 5.5f);
    shader.setUniform("headlampRange", 8500.0f);
    shader.setUniform("headlampConeCos", 0.920504853f);
}

void Scene_IC_Camp::renderOrbs()
{
    auto& window = m_game.window();
    auto& camTransform = m_entityManager.getTransform(m_camera);
    auto& camData = m_entityManager.getCamera(m_camera);
    const sf::Vector2u winSize = m_bakeTexture.getSize();

    struct OrbDrawItem
    {
        sf::Vector2f screenPos;
        sf::Vector3f cameraSpacePos;
        float radiusPx;
        float depthSort;
        float depthNorm;
        sf::Color color;
    };

    std::vector<OrbDrawItem> orbDrawItems;
    orbDrawItems.reserve(8192);

    const float focalLengthPx = (static_cast<float>(winSize.y) * 0.5f) /
                                std::tan(camData.fovY * 0.5f);

    m_entityManager.forEachOrbWithTransform([&](SoAEntityHandle, CTransform3D& orbTransform, COrb& orbData)
    {
        sf::Vector2f screenPos;
        if (!Camera::worldToScreen(camTransform, camData, orbTransform.pos, screenPos))
            return;

        sf::Vector3f relative = orbTransform.pos - camTransform.pos;
        sf::Vector3f cameraSpace = Camera::worldToCamera(
            relative, camTransform.pitch, camTransform.yaw, camTransform.roll);

        if (cameraSpace.z >= -camData.nearPlane) return;

        float depth = std::abs(cameraSpace.z);
        if (depth <= 0.0001f) return;

        float radiusPx = orbData.radius * focalLengthPx / depth;
        if (radiusPx <= 0.5f) return;

        float depthNorm = std::clamp(
            (depth - camData.nearPlane) / (camData.farPlane - camData.nearPlane), 0.0f, 1.0f);

        orbDrawItems.push_back({screenPos, cameraSpace, radiusPx, depth, depthNorm, orbData.color});
    });

    if (orbDrawItems.empty()) return;

    std::sort(orbDrawItems.begin(), orbDrawItems.end(),
        [](const OrbDrawItem& a, const OrbDrawItem& b) { return a.depthSort > b.depthSort; });

    sf::Vector3f sunDirView = Camera::worldToCamera(
        m_sunDirection, camTransform.pitch, camTransform.yaw, camTransform.roll);

    constexpr int BATCH_SIZE = 64;
    OrbBatch batch;
    batch.reserve(BATCH_SIZE);

    sf::VertexArray vertices(sf::PrimitiveType::Triangles, 0);  // We'll append manually

    for (size_t i = 0; i < orbDrawItems.size(); ++i)
    {
        const auto& item = orbDrawItems[i];
        const float r = item.radiusPx;
        const sf::Vector2f origin(item.screenPos.x - r, item.screenPos.y - r);
        const sf::Vector2f size(r * 2.f, r * 2.f);

        // Encode orb index in red channel (0-63)
        uint8_t idx = static_cast<uint8_t>(batch.centersView.size());
        sf::Color indexColor(idx, 0, 0, 255);

        // Two triangles per quad
        sf::Vertex v1(origin, indexColor);
        sf::Vertex v2({origin.x + size.x, origin.y}, indexColor);
        sf::Vertex v3({origin.x + size.x, origin.y + size.y}, indexColor);
        sf::Vertex v4({origin.x, origin.y + size.y}, indexColor);

        vertices.append(v1);
        vertices.append(v2);
        vertices.append(v3);

        vertices.append(v1);
        vertices.append(v3);
        vertices.append(v4);

        // Batch data
        batch.centersView.push_back(item.cameraSpacePos);
        batch.colors.emplace_back(
            item.color.r / 255.f, item.color.g / 255.f,
            item.color.b / 255.f, item.color.a / 255.f);
        batch.depthNorms.push_back(item.depthNorm);
        batch.quadOrigins.push_back(origin);
        batch.texSizes.emplace_back(size);

        // Flush when batch is full or at end
        if (batch.centersView.size() == BATCH_SIZE || i == orbDrawItems.size() - 1)
        {
            uploadOrbBatchToShader(m_orbShader, batch, sunDirView);

            sf::RenderStates states;
            states.shader = &m_orbShader;
            window.draw(vertices, states);

            vertices.clear();
            batch.clear();
        }
    }
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

    // ── Draw the hillshaded topo layer via shader ──────────────────────────
    m_minimapTexture.clear(sf::Color::Transparent);

    sf::RectangleShape fullQuad({texSize, texSize});

    m_topoMinimapShader.setUniform("u_playerXZ",
        sf::Glsl::Vec2(playerPos.x, playerPos.z));
    m_topoMinimapShader.setUniform("u_worldRadius",  worldRadius);
    m_topoMinimapShader.setUniform("u_texSize",      texSize);
    m_topoMinimapShader.setUniform("u_heightMax",    m_topdownMaxHeight);
    m_topoMinimapShader.setUniform("u_reliefExaggeration", 2.4f);
    Scene_IC_Camp::uploadTerrainLayersToShader(m_topoMinimapShader, "u_layers");
    for (int i = 0; i < 16; ++i) {
        m_topoMinimapShader.setUniform("u_activeLayerEnabled[" + std::to_string(i) + "]", 1.0f);
    }
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

    auto& cam = m_entityManager.getTransform(m_camera);
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