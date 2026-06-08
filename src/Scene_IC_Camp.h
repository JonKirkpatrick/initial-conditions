#pragma once
#include "Scene.h"
#include "HUD.h"
#include "Topography.hpp"
#include "SoAEntityManager.hpp"
#include "Astro.hpp"
#include <SFML/System.hpp>

class Scene_IC_Camp : public Scene {

    // =========================================================================
    // Inner Types
    // =========================================================================

    struct CameraConfig
    {
        unsigned int VIEWPORT_WIDTH, VIEWPORT_HEIGHT;
        float POSITION_X, POSITION_Y, POSITION_Z, PITCH, YAW, ROLL, FOVY, NEAR_PLANE, FAR_PLANE;
    };

    struct PlayerConfig
    {
        float MOVE_SPEED, ROTATION_SPEED, HEIGHT_OFFSET, EYE_OFFSET;
        int POSITION_X, POSITION_Z;
    };

    using TerrainLayer = Topography::TerrainLayer;

    enum class HeadlightState { Off, On, Auto };

    struct ShadowOrbEntry {
        sf::Vector3f worldPos;
        float        radius;
    };

    struct OrbDrawItem
    {
        sf::Vector2f screenPos;
        sf::Vector3f cameraSpacePos;
        float        radiusPx;
        float        orbRadius;
        sf::Vector3f worldPos;
        float        distSort;
        float        distNorm;
        sf::Color    color;
    };

    struct OrbBatch
    {
        static constexpr int MAX_BATCH_SIZE = 64;

        std::vector<sf::Vector3f>    centersView;
        std::vector<sf::Glsl::Vec4>  colors;
        std::vector<float>           depthNorms;
        std::vector<sf::Vector2f>    quadOrigins;
        std::vector<sf::Vector2f>    texSizes;

        void clear()
        {
            centersView.clear();
            colors.clear();
            depthNorms.clear();
            quadOrigins.clear();
            texSizes.clear();
        }

        void reserve(size_t n)
        {
            centersView.reserve(n);
            colors.reserve(n);
            depthNorms.reserve(n);
            quadOrigins.reserve(n);
            texSizes.reserve(n);
        }
    };

protected:

    // =========================================================================
    // Core Scene State
    // =========================================================================

    std::string      m_levelPath;
    SoAEntityHandle  m_camera;
    SoAEntityHandle  m_player;
    CameraConfig     m_cameraConfig;
    PlayerConfig     m_playerConfig;

    float        m_hexSize = 100.f;
    sf::Color    m_gridColor;
    sf::Vector2f m_homeLocationXZ{0.f, 0.f};

    // =========================================================================
    // Time, Date and Location
    // =========================================================================

    int    m_gameYear       = 2000;
    int    m_gameMonth      = 1;
    int    m_gameDayOfMonth = 1;
    double m_gameTimeOfDay  = 12.0;
    float  m_latitude       = 0.f;
    float  m_longitude      = 0.f;
    Astro::State m_astroState;

    // =========================================================================
    // Terrain
    // =========================================================================

    std::array<TerrainLayer, 16> m_terrainLayers{};
    uint32_t m_activeLayerMask  = 0xFFFF;
    float    m_warpScale        = 0.00020f;
    float    m_warpStrength     = 2415.0f;
    float    m_topdownMaxHeight = 1.f;
    sf::Vector2f m_topdownWorldMin{0.f, 0.f};
    sf::Vector2f m_topdownWorldSize{1.f, 1.f};

    // =========================================================================
    // Rendering — Shaders
    // =========================================================================

    sf::Shader& m_bakeShader        = Assets::Instance().getShader("TopoBake");
    sf::Shader& m_depthStepShader   = Assets::Instance().getShader("TopoDepthSteps");
    sf::Shader& m_finalShader       = Assets::Instance().getShader("NewFinal");
    sf::Shader& m_topdownShader     = Assets::Instance().getShader("MaxMipBase");
    sf::Shader& m_topoMinimapShader = Assets::Instance().getShader("TopoMiniMap");
    sf::Shader& m_sky               = Assets::Instance().getShader("Sky");
    sf::Shader& m_orbShader         = Assets::Instance().getShader("Orb");

    // =========================================================================
    // Rendering — Render Textures
    // =========================================================================

    sf::RenderTexture m_renderTexture;
    sf::RenderTexture m_bakeTexture;
    sf::RenderTexture m_skyTexture;
    sf::RenderTexture m_topdownTexture;
    sf::Image         m_topdownImage;
    sf::RenderTexture m_minimapTexture;
    sf::RenderTexture m_newBakeTexture;
    unsigned int m_topdownTextureSize  = 512;
    unsigned int m_minimapTextureSize  = 256;

    // New bake pipeline
    GLuint m_gridVAO        = 0;
    GLuint m_gridVBO        = 0;
    GLuint m_gridEBO        = 0;
    GLuint m_gridIndexCount = 0;
    GLuint m_newBakeProgram = 0;
    

    // =========================================================================
    // Rendering — Sky Cubemap
    // =========================================================================

    unsigned int m_skyCubemapHandle = 0;
    bool         m_skyCubemapReady  = false;

    // =========================================================================
    // Rendering — Shader Quality Tuning
    // =========================================================================

    // Overall raymarch quality (0.05..1.0); lower trades fidelity for performance
    float m_shaderQuality = 1.0f;

    // Multiplier for raymarch step size; <1.0 finer detail, >1.0 faster
    float m_stepSizeScale = 1.0f;

    // Distance above cached heightmap before switching to full raymarching
    float m_heightmapTransitionThreshold = 350.0f;

    // Debug: scales step-count visualization channel in the depth/step pass
    float m_stepContributionScale = 1.0f;

    // Debug: fixed divisor for step-count normalization
    float m_stepCountNormalizationMax = 255.0f;

    // =========================================================================
    // Rendering — Sun and Atmosphere
    // =========================================================================

    float m_sunIntensity    = 1.0f;
    float m_atmosphereTint  = 0.0f;

    // =========================================================================
    // Rendering — Moon
    // =========================================================================

    sf::Texture m_moonTexture;

    // =========================================================================
    // Rendering — Orbs
    // =========================================================================

    std::array<OrbDrawItem, 8192> m_orbDrawItems;
    int m_orbDrawItemCount = 0;

    std::vector<ShadowOrbEntry> m_shadowOrbList;

    // =========================================================================
    // Rendering — HUD
    // =========================================================================

    std::unique_ptr<HUD> m_hud;
    HUD_Data             m_hudData;

    // =========================================================================
    // Player and Camera Feel
    // =========================================================================

    sf::Vector3f m_cameraBobOffset{0.f, 0.f, 0.f};
    float m_bobLag      = 0.16f;  // Smoothing factor (0..1)
    float m_crouchFactor = 0.0f;  // 0 = standing, 1 = fully crouched

    float m_lastStepPhase = 0.0f;

    HeadlightState m_headlightState = HeadlightState::Auto;

    // =========================================================================
    // Debug and Editor Flags
    // =========================================================================

    bool m_drawGrid         = true;
    bool m_drawTextures     = true;
    bool m_drawCollision    = false;
    bool m_showGUI          = false;
    bool m_cursorMode       = false;
    bool m_useDepthStepDebug  = false;
    bool m_showTopDownViewer  = false;

    // =========================================================================
    // Performance Tracking
    // =========================================================================

    sf::Clock m_fpsClock;
    int   m_fpsFrameCount  = 0;
    float m_fps            = 0.0f;
    float m_lastFrameTime  = 0.0f;

    // =========================================================================
    // Scene Initialization
    // =========================================================================

    void loadLevel(const std::string& filename);
    void spawnPlayer();
    void spawnCamera();
    void spawnOrb(int hexQ, int hexR, const sf::Color& color, float radius,
                  float bobRate = 2.0f, float bobMagnitude = 8.0f);
    void spawnDebugOrbs(int count);
    void buildHud();
    void buildTerrainGrid();
    void initializeSkyCubemap();

    // =========================================================================
    // Per-Frame Updates
    // =========================================================================

    void updateCamera(float dt);
    void updateHUDData();
    void updateMinimapTexture();
    void updateSiderealTime();
    void updateSunPosition();
    void updateStarRotation();
    void updateMoonPosition();
    void updateShadowOrbs();
    void updateBob(SoAEntityHandle e, float dt, float horizSpeed);
    void updateOrbBobbing(SoAEntityHandle e, float dt);

    // =========================================================================
    // Movement and Physics
    // =========================================================================

    void sMovement(float dt);
    void handlePlayerMovement(SoAEntityHandle player, float dt);
    void resolveEntityPosition(SoAEntityHandle e, float dt);
    bool shouldHeadlightsBeOn() const;

    // =========================================================================
    // Render Passes
    // =========================================================================

    void renderSky(const sf::Glsl::Mat3& worldToCamMatrix);
    void runBakePass(const sf::Glsl::Mat3& worldToCamMatrix);
    void runNewBakePass();
    void runFinalPass(const sf::Glsl::Mat3& worldToCamMatrix);
    void runDepthStepPass(const sf::Glsl::Mat3& worldToCamMatrix);
    void runTopDownPass();
    void renderOrbs();
    void renderTopDownViewer(sf::RenderWindow& window);

    // =========================================================================
    // Shader Uniform Upload
    // =========================================================================

    void uploadTerrainLayersToShader(sf::Shader& shader, const std::string& prefix);
    void uploadActiveLayerMaskToShader(sf::Shader& shader, const std::string& prefix);
    void uploadWarpParametersToShader(sf::Shader& shader) const;
    void uploadShadowOrbsToShader(sf::Shader& shader);
    void uploadOrbBatchToShader(sf::Shader& shader, const OrbBatch& batch,
                                const sf::Vector3f& sunDirView);

    // =========================================================================
    // Terrain Query Helpers
    // =========================================================================

    float heightAt(float x, float z) const;

    float getCameraHeightAboveGround(const sf::Vector3f& cameraPos) const {
        return Topography::getCameraHeightAboveGround(cameraPos, m_terrainLayers, m_activeLayerMask);
    }
    float computeSceneMaxHeight() const {
        return Topography::computeSceneMaxHeight(m_terrainLayers, m_activeLayerMask);
    }
    float evaluateLayerHeightAt(const TerrainLayer& layer, float x, float z) const {
        return Topography::evaluateLayerHeightAt(layer, x, z);
    }
    sf::Vector3f terrainNormal(float x, float z, float epsilon = 50.0f) const {
        return Topography::terrainNormal(x, z, m_terrainLayers, m_activeLayerMask, epsilon);
    }

    float sampleHeightmapAt(float worldX, float worldZ) const;

    // =========================================================================
    // Coordinate and Color Utilities
    // =========================================================================

    sf::Glsl::Vec3 colorToShader(const sf::Color& color);
    sf::Vector2i   worldToHex(float x, float z) const;
    sf::Vector2f   hexToWorld(int q, int r) const;
    sf::Vector3f   screenToWorld(sf::Vector2i mousePos) const;
    void           sortOrbs();

public:

    // =========================================================================
    // Public Interface
    // =========================================================================

    Scene_IC_Camp(GameEngine& game, const std::string& levelPath);
    void sGUI();
    void update();
    void sDoAction(const Action& action);
    void sRender();
    void onEnter();
    void onExit();
    void onEnd();
    HUD* getHUD() const override;
};