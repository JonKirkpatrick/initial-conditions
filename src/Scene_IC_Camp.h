#pragma once
#include "Scene.h"
#include "HUD.h"
#include <SFML/System.hpp>

class Scene_IC_Camp : public Scene {

    struct CameraConfig
    {
        unsigned int VIEWPORT_WIDTH, VIEWPORT_HEIGHT;
        float POSITION_X, POSITION_Y, POSITION_Z, PITCH, YAW, ROLL, FOVY, NEAR_PLANE, FAR_PLANE;
    };

    struct PlayerConfig
    {
        float MOVE_SPEED, ROTATION_SPEED, HEIGHT_OFFSET, EYE_OFFSET, POSITION_X, POSITION_Z;
    };

    struct TerrainLayer
    {
        sf::Vector2f center;
        float radius;
        float falloffWidth;
        float topoScale;
        float frequency;
        float boundaryHeight;
    };

    typedef enum class HeadlightState
    {
        Off,
        On,
        Auto
    } HeadlightState;
    
protected:
    std::string m_levelPath;
    std::shared_ptr<Entity> m_camera;
    std::shared_ptr<Entity> m_player;
    CameraConfig m_cameraConfig;
    PlayerConfig m_playerConfig;
    std::unique_ptr<HUD>    m_hud;
    HUD_Data m_hudData;

    // Debug visualization options
    bool m_drawGrid = true;
    bool m_drawTextures = true;
    bool m_drawCollision = false;
    bool m_showGUI = true;
    bool m_cursorMode = false;
    HeadlightState m_headlightState = HeadlightState::Auto;

    // Scene-specific data
    float m_hexSize = 100.f;
    sf::Color m_gridColor;

    sf::Vector2f m_homeLocationXZ{0.f, 0.f}; // Move to level file??

    // Shaders and render textures
    sf::Shader& m_bakeShader = Assets::Instance().getShader("TopoBake");
    sf::Shader& m_finalShader = Assets::Instance().getShader("TopoFinal");
    sf::Shader& m_sky = Assets::Instance().getShader("Sky");
    sf::RenderTexture m_renderTexture;
    sf::RenderTexture m_bakeTexture;
    sf::RenderTexture m_skyTexture;
    sf::RenderTexture m_minimapTexture;
    sf::Image m_currentBakeImage;
    float m_minimapWorldRadius = 10000.f;
    unsigned int m_minimapTextureSize = 256;
    float m_minimapContourStep = 300.f;

    // Camera bob smoothing (lag) state
    sf::Vector3f m_cameraBobOffset{0.f, 0.f, 0.f};
    float m_bobLag = 0.16f; // smoothing factor for camera bob (0..1)

    // FPS display
    sf::Clock m_fpsClock;
    int m_fpsFrameCount = 0;
    float m_fps = 0.0f;
    // Last frame wall-clock time (seconds) for delta-time
    float m_lastFrameTime = 0.0f;

    //---- */ Helpers and members to setup up shader uniforms ----//
    // Shader render quality (0.05 .. 1.0) - lower reduces GPU work at cost of visual fidelity
    float m_shaderQuality = 1.0f;
    // Step size scale (0.1 .. 5.0) - lower for finer detail, higher for performance
    float m_stepSizeScale = 1.0f;
    // Active layer bitmask for culling (bit i = layer i enabled)
    uint32_t m_activeLayerMask = 0xFFFF;
    sf::Glsl::Vec3 colorToShader(const sf::Color& color);
    sf::Vector2i worldToHex(float x, float z) const;
    sf::Vector2f hexToWorld(int q, int r) const;
    sf::Vector3f screenToWorld(sf::Vector2i mousePos) const;
    void uploadActiveLayerMaskToShader(sf::Shader& shader, const std::string& prefix);


    float m_gameTimeOfDay; // Hours between 0 and 24
    int m_gameDayOfYear; // Day of the year between 1 and 365
    float m_latitude; // Newfoundland

    // Sun parameters
    sf::Glsl::Vec3 m_sunDirection;
    sf::Glsl::Vec4 m_sunColor;
    float m_sunIntensity = 1.0f;
    float m_atmosphereTint = 0.0f;
    // Terrain layer system (16 regions, additive composition)
    std::array<TerrainLayer, 16> m_terrainLayers;
    void initializeTerrainLayers();
    void uploadTerrainLayersToShader(sf::Shader& shader, const std::string& prefix);
    uint32_t computeActiveLayerMask(const sf::Vector3f& cameraPos);
    float evaluateLayerHeightAt(const TerrainLayer& layer, float x, float z) const;


    void updateSunPosition();
    bool shouldHeadlightsBeOn() const;

    // Physical bridge for terrain
    float heightAt(float x, float z) const;
    void updateCamera();
    void captureBake();

    // Main render passes and world updates
    void updateHUDData();
    void updateMinimapTexture();
    void buildHud();
    void runBakePass();
    void runFinalPass();
    void renderWorld();

public:
    Scene_IC_Camp(GameEngine& game, const std::string& levelPath);
    void sGUI();
    void update();
    void sDoAction(const Action& action);
    void sRender();
    void onEnter();
    void onExit();
    void onEnd();
    void sMovement(float dt);
    void spawnCamera();
    void spawnPlayer();
    void loadLevel(const std::string& filename);
    HUD* getHUD() const override;
};
