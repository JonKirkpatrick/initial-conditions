#pragma once
#include <GL/glew.h>
#include "scenes/Scene.h"
#include "renderer/OrbSSBO.h"
#include "renderer/RenderTypes.h"
#include "ui/HUD.h"
#include "environment/Topography.h"
#include "ecs/SoAEntityManager.hpp"
#include "environment/Astro.hpp"
#include <SFML/System.hpp>

class TerrainStreamer;

class Scene_IC_Camp : public Scene {

    // =========================================================================
    // Inner Types
    // =========================================================================

    struct CameraConfig
    {
        unsigned int VIEWPORT_WIDTH, VIEWPORT_HEIGHT;
        float FOVY, NEAR_PLANE, FAR_PLANE;
    };

    struct PlayerConfig
    {
        float MOVE_SPEED, ROTATION_SPEED, HEIGHT_OFFSET, EYE_OFFSET;
        int POSITION_X, POSITION_Z;
    };

    enum class HeadlightState { Off, On, Auto };

    struct SSAOPipeline {
        GLuint fbo = 0;
        GLuint blurFBO = 0;
        GLuint colorTex = 0;
        GLuint blurTex = 0;
        GLuint noiseTex = 0;

        void destroy() {
            if (fbo)     glDeleteFramebuffers(1, &fbo);
            if (blurFBO) glDeleteFramebuffers(1, &blurFBO);
            if (colorTex) glDeleteTextures(1, &colorTex);
            if (blurTex)  glDeleteTextures(1, &blurTex);
            if (noiseTex) glDeleteTextures(1, &noiseTex);
            fbo = blurFBO = colorTex = blurTex = noiseTex = 0;
        }
    };

protected:

    // =========================================================================
    // Core Scene State
    // =========================================================================

    std::string         m_levelPath;
    SoAEntityHandle     m_camera;
    SoAEntityHandle     m_player;
    CameraConfig        m_cameraConfig;
    PlayerConfig        m_playerConfig;
    std::unique_ptr<TerrainStreamer> m_terrainStreamer;
    sf::Vector2i        m_cachedMousePos{0,0};
    bool                m_leftMousePressed = false;

    float               m_hexSize = 1.f;
    sf::Color           m_gridColour;
    sf::Vector2f        m_homeLocationXZ{0.f, 0.f};
    sf::Vector3f        m_homeLocation3D{0.f, 0.f, 0.f};

    // =========================================================================
    // Time, Date and Location
    // =========================================================================

    int                 m_gameYear              = 2000;
    int                 m_gameMonth             = 1;
    int                 m_gameDayOfMonth        = 1;
    double              m_gameTimeOfDay         = 12.0;
    float               m_latitude              = 0.f;
    float               m_longitude             = 0.f;
    Astro::State        m_astroState;

    // =========================================================================
    // Terrain
    // =========================================================================

    float               m_topdownMaxHeight      = 1.f;
    sf::Vector2f        m_topdownWorldMin{0.f, 0.f};
    sf::Vector2f        m_topdownWorldSize{1.f, 1.f};

    // =========================================================================
    // Ocean
    // =========================================================================

    float m_oceanSize = 10000.0f;
    float m_oceanResolution = 200.0f;
    
    // =========================================================================
    // Rendering — OpenGL Programs
    // =========================================================================

    GLuint              m_minimapProgram        = Assets::Instance().getGLProgram("MiniMap");
    GLuint              m_skyProgram            = Assets::Instance().getGLProgram("Sky");
    GLuint              m_ssao                  = Assets::Instance().getGLProgram("SSAO");
    GLuint              m_ssao_blur             = Assets::Instance().getGLProgram("SSAOBlur");
    GLuint              m_terrainProgram        = Assets::Instance().getGLProgram("Terrain");
    GLuint              m_OrbCreatureProgram    = Assets::Instance().getGLProgram("OrbCreature");
    GLuint              m_blitProgram           = Assets::Instance().getGLProgram("Blit");
    GLuint              m_lightingProgram       = Assets::Instance().getGLProgram("Lighting");
    GLuint              m_terrainShadowProgram  = Assets::Instance().getGLProgram("TerrainShadow");
    GLuint              m_orbShadowProgram      = Assets::Instance().getGLProgram("OrbShadow");
    GLuint              m_oceanProgram          = Assets::Instance().getGLProgram("Ocean");


    // =========================================================================
    // Rendering — Render Textures and OpenGL Resources
    // =========================================================================

    sf::RenderTexture   m_skyTexture;
    sf::RenderTexture   m_minimapTexture;
    unsigned int        m_minimapTextureSize    = 256;

    // ocean pipeline
    GLuint              m_oceanVAO              = 0;
    GLuint              m_oceanVBO              = 0;
    GLuint              m_oceanEBO              = 0;
    GLuint              m_oceanIndexCount       = 0;

    // terrain pipeline
    GLuint              m_gridVAO               = 0;
    GLuint              m_gridVBO               = 0;
    GLuint              m_gridEBO               = 0;
    GLuint              m_gridIndexCount        = 0;

    // sphere impostor pipeline
    GLuint              m_cubeVAO               = 0;
    GLuint              m_cubeVBO               = 0;
    GLuint              m_cubeEBO               = 0;

    // G-Buffer for deferred lighting
    GLuint              m_gBufferFBO            = 0;
    GLuint              m_gAlbedoTex            = 0;
    GLuint              m_gNormalTex            = 0;
    GLuint              m_gIndicesTex           = 0;
    GLuint              m_gRetroTex             = 0;
    GLuint              m_gDepthTex             = 0;

    unsigned int        m_gBufferWidth          = 0;
    unsigned int        m_gBufferHeight         = 0;

    // SSAO Textures and Framebuffers
    SSAOPipeline        m_ssaoPipeline;
    std::vector<sf::Glsl::Vec3> m_ssaoKernel;

    // Uniform Buffer Objects (UBOs) for camera and lighting data
    GLuint              m_cameraUBO             = 0;
    GLuint              m_envUBO                = 0;
    GLuint              m_atmoUBO               = 0;


    // =========================================================================
    // Rendering — Blit
    // =========================================================================

    GLuint              m_blitVAO               = 0;
    GLuint              m_blitVBO               = 0;

    // =========================================================================
    // Rendering — Deferred Lighting
    // =========================================================================

    GLuint              m_lightingVAO               = 0;
    GLuint              m_lightingVBO               = 0;

    // =========================================================================
    // Rendering — Shadow Map
    // =========================================================================

    GLuint                  m_shadowFBO             = 0;
    GLuint                  m_shadowDepthTexArray   = 0;
    unsigned int            m_shadowMapSize         = 4096;
    static constexpr int    NUM_CASCADES            = 5;

    float                   m_cascadeSplits[NUM_CASCADES] = {};
    glm::mat4               m_lightViewProjCascades[NUM_CASCADES] = {};
    float                   m_lightDepthRange[NUM_CASCADES] = {};
    float                   m_texelWorldSize[NUM_CASCADES] = {};
    float                   m_cascadeSplitLambda    = 0.25f; // 0 = uniform, 1 = logarithmic
    float                   m_shadowFrustumPadding  = 30.0f;
    float                   m_shadowMaxDistance     = 500.0f;

    bool                    m_debugShowShadowMap     = false;
    bool                    m_debugDisableTexelSnap  = false;
    bool                    m_debugShowCascadeColors = false;

    // =========================================================================
    // Rendering — Sky Cubemap
    // =========================================================================

    unsigned int        m_skyCubemapHandle      = 0;
    bool                m_skyCubemapReady       = false;

    // =========================================================================
    // Rendering — Sun and Atmosphere
    // =========================================================================

    float               m_sunIntensity          = 1.0f;
    float               m_atmosphereTint        = 0.0f;

    // =========================================================================
    // Rendering — Moon
    // =========================================================================

    sf::Texture         m_moonTexture;

    // =========================================================================
    // Rendering — Orbs
    // =========================================================================

    OrbSSBO             m_orbSSBO;
    std::vector<OrbData> buildOrbData() const;

    // =========================================================================
    // Rendering — HUD
    // =========================================================================

    std::unique_ptr<HUD> m_hud;
    HUD_Data             m_hudData;

    // =========================================================================
    // Player and Camera Feel
    // =========================================================================

    sf::Vector3f m_cameraBobOffset{0.f, 0.f, 0.f};
    float m_bobLag                              = 0.16f;  // Smoothing factor (0..1)
    float m_crouchFactor                        = 0.0f;  // 0 = standing, 1 = fully crouched

    float m_lastStepPhase                       = 0.0f;

    HeadlightState m_headlightState             = HeadlightState::Auto;

    // =========================================================================
    // Debug and Editor Flags
    // =========================================================================

    bool m_drawGrid         = true;
    bool m_drawTextures     = true;
    bool m_drawCollision    = false;
    bool m_showGUI          = false;
    bool m_cursorMode       = false;
    glm::vec3 m_nightAmbientFloor = glm::vec3(0.002f, 0.003f, 0.006f);
    bool m_debugShowSSAO     = false;
    bool m_debugShowSSAOBlur = false;
    float m_debugSSAOKernelRadius = 0.25f;
    float m_debugSSAOBias         = 0.005f;
    int m_sampleCount = 16;
    int m_ssaoKernelSize = 16;

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
    void spawnOrbFauna(int hexQ, int hexR, float radius,
                      float bobRate = 2.0f, float bobMagnitude = 8.0f,
                      const CEyes& eyes = CEyes(), float yaw = 0.0f, int species = 0);
    void spawnDebugOrbs(int count);
    void buildHud();
    void buildTerrainGrid();
    void buildVertexCube();
    void generateOceanMesh(float size, int resolution, unsigned int& vao, unsigned int& vbo, unsigned int& ebo, unsigned int& indexCount);
    void initializeSkyCubemap();
    void initializeOrbShaderStorage();
    void initializeGBuffer(unsigned int width, unsigned int height);
    void initSSAONoiseTexture();
    void initSSAOKernel();
    void initSSAOFramebuffers();
    void destroyGBuffer();
    void initializeShadowMap(unsigned int size);
    void destroyShadowMap();
    void initUBOs();
    void initSceneConfiguration();
    void initGraphicsPipelines();
    void initLevelState();
    void cleanUpGraphicsResources();

    // =========================================================================
    // Per-Frame Updates
    // =========================================================================

    void updateCamera(float dt);
    void updateHUDData();
    void updateMinimapTexture();
    void updateAstronomySystem();
    void updateBob(SoAEntityHandle e, float dt, float horizSpeed);
    void updateOrbBobbing(SoAEntityHandle e, float dt);
    glm::mat4 computeLightViewProjForRange(float splitNear, float splitFar, float& lightDepthRange, float& texelWorldSize) const;
    glm::mat4 computeLightViewProjForMapBounds(float& lightDepthRange, float& texelWorldSize) const;
    float computeDistanceToMapFarCorner(const glm::vec3& cameraPos) const;
    void computeCascadeSplits(float camNear, float camFar);
    void uploadViewRays(GLuint shaderProgram);

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

    void runShadowPass();
    void runTerrainPass();
    void renderOrbCreature();
    void renderSky();
    void runSSAOPass();
    void runSSAOBlurPass();
    void deferredLighting();
    void blitToScreen(GLuint tex);
    void renderOceanGrid();

    // =========================================================================
    // Shader Uniform Upload
    // =========================================================================

    void updateOrbShaderStorage();

    // =========================================================================
    // Terrain Query Helpers
    // =========================================================================

    float getCameraHeightAboveGround(const sf::Vector3f& cameraPos) const;
    float heightAt(float x, float z) const {
        return Topography::heightAt(getTerrainContext(), sf::Vector2f(x, z));
    }
    sf::Vector3f normalAt(float x, float z) const {
        return Topography::normalAt(getTerrainContext(), sf::Vector2f(x, z));
    }

    // =========================================================================
    // Coordinate and Color Utilities
    // =========================================================================

    sf::Glsl::Vec3 colorToShader(const sf::Color& color);
    sf::Vector3f   screenToWorld(sf::Vector2i mousePos) const;

public:

    // =========================================================================
    // Public Interface
    // =========================================================================

    Scene_IC_Camp(GameEngine& game, const std::string& levelPath);
    ~Scene_IC_Camp() override;
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