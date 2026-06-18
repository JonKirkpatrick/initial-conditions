#pragma once
#include "Scene.h"
#include "HUD.h"
#include "Topography.h"
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
        sf::Vector3f gazeDirection;
        sf::Vector3f forward;
        float        hasTapetum;
        sf::Vector3f tapetumColor;
        float        pupilDilation;
        float        eyelidClosure;
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
        std::vector<sf::Vector2f>    quadSizes;
        std::vector<sf::Vector3f>    gazes;
        std::vector<sf::Vector3f>    forwards;
        std::vector<float>           hasTapetums;
        std::vector<sf::Vector3f>    tapetumColors;
        std::vector<float>           pupilDilations;
        std::vector<float>           eyelidClosures;
        void clear()
        {
            centersView.clear();
            colors.clear();
            depthNorms.clear();
            quadOrigins.clear();
            texSizes.clear();
            quadSizes.clear();
            gazes.clear();
            forwards.clear();
            hasTapetums.clear();
            tapetumColors.clear();
            pupilDilations.clear();
            eyelidClosures.clear();
        }

        void reserve(size_t n)
        {
            centersView.reserve(n);
            colors.reserve(n);
            depthNorms.reserve(n);
            quadOrigins.reserve(n);
            texSizes.reserve(n);
            quadSizes.reserve(n);
            gazes.reserve(n);
            forwards.reserve(n);
            hasTapetums.reserve(n);
            tapetumColors.reserve(n);
            pupilDilations.reserve(n);
            eyelidClosures.reserve(n);
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

    float    m_topdownMaxHeight = 1.f;
    sf::Vector2f m_topdownWorldMin{0.f, 0.f};
    sf::Vector2f m_topdownWorldSize{1.f, 1.f};

    // =========================================================================
    // Rendering — Shaders
    // =========================================================================

    sf::Shader& m_terrainShader     = Assets::Instance().getShader("Terrain");
    sf::Shader& m_topoMinimapShader = Assets::Instance().getShader("TopoMiniMap");
    sf::Shader& m_sky               = Assets::Instance().getShader("Sky");
    sf::Shader& m_orbShader         = Assets::Instance().getShader("Orb");

    // =========================================================================
    // Rendering — Render Textures and OpenGL Resources
    // =========================================================================

    sf::RenderTexture m_renderTexture;
    sf::RenderTexture m_skyTexture;
    sf::Texture m_topdownTexture;
    sf::Image         m_topdownImage;
    sf::RenderTexture m_minimapTexture;
    sf::RenderTexture m_bakeTexture;
    sf::Texture       m_bakeSFTexture;
    sf::Texture       m_wolfTexture;
    sf::Texture       m_wolfHeight;
    unsigned int m_minimapTextureSize  = 256;

    // New bake pipeline
    GLuint m_bakeFBO        = 0;
    GLuint m_bakeColorTex   = 0;
    GLuint m_bakeDepthRBO   = 0;
    GLuint m_gridVAO        = 0;
    GLuint m_gridVBO        = 0;
    GLuint m_gridEBO        = 0;
    GLuint m_gridIndexCount = 0;
    GLuint m_bakeProgram    = Assets::Instance().getGLProgram("Bake");

    // New sphere impostor pipeline
    GLuint m_demoSphereProgram = Assets::Instance().getGLProgram("DemoSphere");
    

    // =========================================================================
    // Rendering — Sky Cubemap
    // =========================================================================

    unsigned int m_skyCubemapHandle = 0;
    bool         m_skyCubemapReady  = false;

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
    void spawnOrbFauna(int hexQ, int hexR, const sf::Color& color, float radius,
                      float bobRate = 2.0f, float bobMagnitude = 8.0f, const CEyes& eyes = CEyes());
    void spawnDebugOrbs(int count);
    void buildHud();
    void buildTerrainGrid();
    void initializeSkyCubemap();
    void initializeBakeFBO();
    void debugDumpBakeTexture();

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
    void runBakePass();
    void runTerrainPass(const sf::Glsl::Mat3& worldToCamMatrix);
    void renderOrbs();
    void renderDemoSphere();

    // =========================================================================
    // Shader Uniform Upload
    // =========================================================================

    void uploadShadowOrbsToShader(sf::Shader& shader);
    void uploadOrbBatchToShader(sf::Shader& shader, const OrbBatch& batch,
                                const sf::Vector3f& sunDirView);

    // =========================================================================
    // Terrain Query Helpers
    // =========================================================================

    float getCameraHeightAboveGround(const sf::Vector3f& cameraPos) const;
    float heightAt(float x, float z) const {
        return Topography::heightAt(getTerrainContext(), x, z);
    }
    sf::Vector3f normalAt(float x, float z) const {
        return Topography::normalAt(getTerrainContext(), x, z);
    }

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
    Topography::TerrainContext getTerrainContext() const;
};