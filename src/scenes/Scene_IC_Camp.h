/**
 * @file Scene_IC_Camp.h
 * @brief Main interactive campaign scene class ("IC Camp").
 * 
 * Implements a full 3D environment scene integrating terrain streaming, dynamic astronomical illumination, 
 * ocean mesh synthesis, multi-cascade shadow mapping (CSM), deferred G-Buffer rendering with SSAO, 
 * and entity-component-system (SoA) orb creature fauna streaming.
 */

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

/**
 * @brief Interactive campaign campsite scene class managing physics, rendering pipelines, and UI.
 */
class Scene_IC_Camp : public Scene {

    // =========================================================================
    // Inner Types
    // =========================================================================

    /**
     * @brief Camera projection and viewport configuration parameters.
     */
    struct CameraConfig
    {
        unsigned int VIEWPORT_WIDTH;  ///< Viewport pixel width.
        unsigned int VIEWPORT_HEIGHT; ///< Viewport pixel height.
        float FOVY;                   ///< Vertical field of view in degrees.
        float NEAR_PLANE;             ///< Near clipping plane distance.
        float FAR_PLANE;              ///< Far clipping plane distance.
    };

    /**
     * @brief Player motion and initial spawn properties.
     */
    struct PlayerConfig
    {
        float MOVE_SPEED;     ///< Movement speed in units per second.
        float ROTATION_SPEED; ///< Turn/yaw rotation rate scalar.
        float HEIGHT_OFFSET;  ///< Vertical offset above ground surface.
        float EYE_OFFSET;     ///< Camera eye elevation relative to player root.
        int POSITION_X;       ///< Initial X grid coordinate for spawn.
        int POSITION_Z;       ///< Initial Z grid coordinate for spawn.
    };

    /**
     * @brief Control mode for player headlamp illumination.
     */
    enum class HeadlightState { Off, On, Auto };

    /**
     * @brief Screen-Space Ambient Occlusion (SSAO) framebuffer and texture handle container.
     */
    struct SSAOPipeline {
        GLuint fbo = 0;      ///< Framebuffer object handle for raw SSAO pass.
        GLuint blurFBO = 0;  ///< Framebuffer object handle for SSAO blur pass.
        GLuint colorTex = 0; ///< 2D texture storing unblurred ambient occlusion values.
        GLuint blurTex = 0;  ///< 2D texture storing spatially blurred ambient occlusion values.
        GLuint noiseTex = 0; ///< 4x4 tiled noise texture storing random rotation vectors.

        /**
         * @brief Destroys and cleans up OpenGL resources allocated for the SSAO pipeline.
         */
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

    std::string         m_levelPath;                                ///< Directory path to level assets and configuration.
    SoAEntityHandle     m_camera;                                   ///< ECS entity handle for active camera.
    SoAEntityHandle     m_player;                                   ///< ECS entity handle for player character.
    CameraConfig        m_cameraConfig;                             ///< Projection settings for primary view camera.
    PlayerConfig        m_playerConfig;                             ///< Kinematic and collision settings for player.
    std::unique_ptr<TerrainStreamer> m_terrainStreamer;             ///< Asynchronous terrain streaming subsystem manager.
    sf::Vector2i        m_cachedMousePos{0,0};                     ///< Last recorded screen mouse position in pixels.
    bool                m_leftMousePressed                          = false; ///< Mouse button state flag.

    float               m_hexSize                                   = 1.f;   ///< Tactical hex grid cell radius scale.
    sf::Color           m_gridColour;                               ///< Render color for hex grid overlay lines.
    sf::Vector2f        m_homeLocationXZ{0.f, 0.f};                 ///< 2D home base spawn point in world space.
    sf::Vector3f        m_homeLocation3D{0.f, 0.f, 0.f};              ///< 3D home base spawn point in world space.

    // =========================================================================
    // Time, Date and Location
    // =========================================================================

    int                 m_gameYear                                  = 2000;  ///< In-game calendar year.
    int                 m_gameMonth                                 = 1;     ///< In-game calendar month [1-12].
    int                 m_gameDayOfMonth                            = 1;     ///< In-game calendar day of month [1-31].
    double              m_gameTimeOfDay                             = 12.0;  ///< In-game diurnal time in decimal hours [0.0 - 24.0).
    float               m_latitude                                  = 0.f;   ///< Geographic latitude in degrees.
    float               m_longitude                                 = 0.f;  ///< Geographic longitude in degrees.
    Astro::State        m_astroState;                               ///< Calculated celestial positions (Sun, Moon, Star matrix).

    // =========================================================================
    // Terrain
    // =========================================================================

    float               m_topdownMaxHeight                          = 1.f;       ///< Elevation scale normalization maximum.
    sf::Vector2f        m_topdownWorldMin{0.f, 0.f};                 ///< Minimum top-down world boundary corner.
    sf::Vector2f        m_topdownWorldSize{1.f, 1.f};                ///< Size dimensions of top-down world area.

    // =========================================================================
    // Ocean
    // =========================================================================

    float               m_oceanSize                                 = 10000.0f; ///< Total side length scale for ocean mesh plane.
    float               m_oceanResolution                           = 200.0f;   ///< Grid resolution step count for ocean mesh generation.
    float               m_seaLevel                                  = 4.5f;     ///< World height elevation defining sea level plane.

    // =========================================================================
    // Wind
    // =========================================================================

    sf::Vector2f        m_windDirection{1.f, 0.f};                 ///< Normalized 2D world space wind direction.
    float               m_windSpeed                                 = 2.0f;     ///< World wind speed magnitude.

    // =========================================================================
    // Rendering — OpenGL Programs
    // =========================================================================

    GLuint              m_minimapProgram        = Assets::Instance().getGLProgram("MiniMap");       ///< Shader program for radar minimap.
    GLuint              m_skyProgram            = Assets::Instance().getGLProgram("Sky");           ///< Shader program for skybox/celestial dome.
    GLuint              m_ssao                  = Assets::Instance().getGLProgram("SSAO");          ///< Shader program for SSAO screen-space evaluation.
    GLuint              m_ssao_blur             = Assets::Instance().getGLProgram("SSAOBlur");      ///< Shader program for SSAO spatial bilateral blur.
    GLuint              m_terrainProgram        = Assets::Instance().getGLProgram("Terrain");       ///< Shader program for terrain surface.
    GLuint              m_OrbCreatureProgram    = Assets::Instance().getGLProgram("OrbCreature");   ///< Shader program for instanced orb creatures.
    GLuint              m_blitProgram           = Assets::Instance().getGLProgram("Blit");          ///< Shader program for full-screen texture blitting.
    GLuint              m_lightingProgram       = Assets::Instance().getGLProgram("Lighting");      ///< Shader program for deferred lighting accumulation.
    GLuint              m_terrainShadowProgram  = Assets::Instance().getGLProgram("TerrainShadow"); ///< Shader program for terrain shadow map depth render.
    GLuint              m_orbShadowProgram      = Assets::Instance().getGLProgram("OrbShadow");     ///< Shader program for orb creature shadow depth render.
    GLuint              m_oceanProgram          = Assets::Instance().getGLProgram("Ocean");         ///< Shader program for dynamic ocean surface waves.


    // =========================================================================
    // Rendering — Render Textures and OpenGL Resources
    // =========================================================================

    sf::RenderTexture   m_skyTexture;                               ///< SFML render texture used for sky processing.
    sf::RenderTexture   m_minimapTexture;                           ///< SFML render texture backing tactical minimap display.
    unsigned int        m_minimapTextureSize                        = 256;  ///< Resolution dimension for minimap texture.

    // ocean pipeline
    GLuint              m_oceanVAO                                  = 0;    ///< Ocean vertex array object.
    GLuint              m_oceanVBO                                  = 0;    ///< Ocean vertex buffer object.
    GLuint              m_oceanEBO                                  = 0;    ///< Ocean element index buffer object.
    GLuint              m_oceanIndexCount                           = 0;    ///< Total index count for rendering ocean grid mesh.

    // terrain pipeline
    GLuint              m_gridVAO                                   = 0;    ///< Terrain mesh vertex array object.
    GLuint              m_gridVBO                                   = 0;    ///< Terrain mesh vertex buffer object.
    GLuint              m_gridEBO                                   = 0;    ///< Terrain mesh element index buffer object.
    GLuint              m_gridIndexCount                            = 0;    ///< Total index count for terrain rendering.

    // sphere impostor pipeline
    GLuint              m_cubeVAO                                   = 0;    ///< Impostor cube vertex array object.
    GLuint              m_cubeVBO                                   = 0;    ///< Impostor cube vertex buffer object.
    GLuint              m_cubeEBO                                   = 0;    ///< Impostor cube index buffer object.

    // G-Buffer for deferred lighting
    GLuint              m_gBufferFBO                                = 0;    ///< Framebuffer handle for G-Buffer pass.
    GLuint              m_gAlbedoTex                                = 0;    ///< G-Buffer albedo & specular 2D render texture.
    GLuint              m_gNormalTex                                = 0;    ///< G-Buffer world normal 2D render texture.
    GLuint              m_gIndicesTex                               = 0;    ///< G-Buffer material ID / index 2D render texture.
    GLuint              m_gRetroTex                                 = 0;    ///< G-Buffer retro-reflective / custom payload texture.
    GLuint              m_gDepthTex                                 = 0;    ///< G-Buffer hardware depth attachment texture.

    unsigned int        m_gBufferWidth                              = 0;    ///< Active width dimension of G-Buffer attachments.
    unsigned int        m_gBufferHeight                             = 0;    ///< Active height dimension of G-Buffer attachments.

    // SSAO Textures and Framebuffers
    SSAOPipeline        m_ssaoPipeline;                             ///< Container holding SSAO textures and framebuffers.
    std::vector<sf::Glsl::Vec3> m_ssaoKernel;                       ///< Sample kernel hemisphere vectors for SSAO.

    // Uniform Buffer Objects (UBOs) for camera and lighting data
    GLuint              m_cameraUBO                                 = 0;    ///< UBO handle for `CameraBlock` memory.
    GLuint              m_envUBO                                    = 0;    ///< UBO handle for `EnvironmentBlock` memory.
    GLuint              m_atmoUBO                                   = 0;    ///< UBO handle for `AtmosphereBlock` memory.


    // =========================================================================
    // Rendering — Blit
    // =========================================================================

    GLuint              m_blitVAO                                   = 0;    ///< Full-screen quad vertex array object.
    GLuint              m_blitVBO                                   = 0;    ///< Full-screen quad vertex buffer object.

    // =========================================================================
    // Rendering — Deferred Lighting
    // =========================================================================

    GLuint              m_lightingVAO                               = 0;    ///< Screen quad VAO for deferred lighting pass.
    GLuint              m_lightingVBO                               = 0;    ///< Screen quad VBO for deferred lighting pass.

    // =========================================================================
    // Rendering — Shadow Map
    // =========================================================================

    GLuint                  m_shadowFBO                             = 0;    ///< Shadow map depth generation framebuffer.
    GLuint                  m_shadowDepthTexArray                   = 0;    ///< 2D Texture Array (`GL_TEXTURE_2D_ARRAY`) for CSM cascade depth maps.
    unsigned int            m_shadowMapSize                         = 2048; ///< Resolution dimension per cascade shadow slice.
    static constexpr int    NUM_CASCADES                            = 5;    ///< Total number of active shadow map cascades.

    float                   m_cascadeSplits[NUM_CASCADES] = {};             ///< Calculated view-space split distance thresholds.
    glm::mat4               m_lightViewProjCascades[NUM_CASCADES]   = {};   ///< Light View-Projection matrices per shadow cascade.
    float                   m_lightDepthRange[NUM_CASCADES]         = {};   ///< Orthographic depth bounds per cascade.
    float                   m_texelWorldSize[NUM_CASCADES]          = {};   ///< Texel side length in world space per cascade.
    float                   m_cascadeSplitLambda                    = 0.25f;///< Logarithmic vs uniform split interpolation factor $[0, 1]$.
    float                   m_shadowFrustumPadding                  = 10.0f;///< Frustum padding margin to prevent edge clipping artifacts.
    float                   m_shadowMaxDistance                     = 500.0f;///< Maximum shadow rendering distance cut-off.

    bool                    m_debugShowShadowMap                    = false;///< Debug flag to display shadow map texture array on HUD.
    bool                    m_debugDisableTexelSnap                 = false;///< Debug flag disabling shadow matrix texel snapping.
    bool                    m_debugShowCascadeColors                = false;///< Debug flag tinting terrain fragments by cascade index.

    enum RenderPass {
        PASS_TERRAIN = 0,
        PASS_ORB_CREATURE,
        PASS_SHADOW,
        PASS_SSAO,
        PASS_SSAO_BLUR,
        PASS_SKY,
        PASS_DEFERRED_LIGHTING,
        PASS_OCEAN,
        PASS_HUD,
        PASS_COUNT
    };

    GLuint m_gpuQueries[2][PASS_COUNT] = {};
    bool     m_gpuQueriesInitialized  = false;
    uint32_t m_queryFrameIdx          = 0;

    // GPU Execution Timers (ms)
    float m_profGpuTerrainMs          = 0.0f;
    float m_profGpuOrbCreatureMs       = 0.0f;
    float m_profGpuShadowMs           = 0.0f;
    float m_profGpuSSAOMs             = 0.0f;
    float m_profGpuSSAOBlurMs         = 0.0f;
    float m_profGpuSkyMs              = 0.0f;
    float m_profGpuDeferredLightingMs = 0.0f;
    float m_profGpuOceanMs            = 0.0f;
    float m_profGpuHUDMs              = 0.0f;

    float m_profTerrainMs          = 0.0f;
    float m_profOrbCreatureMs       = 0.0f;
    float m_profShadowMs           = 0.0f;
    float m_profSSAOMs             = 0.0f;
    float m_profSSAOBlurMs         = 0.0f;
    float m_profSkyMs              = 0.0f;
    float m_profDeferredLightingMs = 0.0f;
    float m_profOceanMs            = 0.0f;
    float m_profHUDMs              = 0.0f;
    float m_profMovementMs         = 0.0f;

    // =========================================================================
    // Rendering — Sky Cubemap
    // =========================================================================

    unsigned int        m_skyCubemapHandle      = 0;     ///< Handle to uploaded OpenGL sky cubemap texture.
    bool                m_skyCubemapReady       = false; ///< Availability flag for sky cubemap.

    // =========================================================================
    // Rendering — Sun and Atmosphere
    // =========================================================================

    float               m_sunIntensity          = 1.0f;  ///< Sun lighting intensity multiplier.
    float               m_atmosphereTint        = 0.0f;  ///< Atmospheric scattering tint scalar.

    // =========================================================================
    // Rendering — Moon
    // =========================================================================

    sf::Texture         m_moonTexture;                  ///< Moon surface diffuse texture sampler.

    // =========================================================================
    // Rendering — Orbs
    // =========================================================================

    /**
     * @brief Builds contiguous array of `OrbData` structures from ECS entity components.
     * @return Vector of formatted `OrbData` structs ready for GPU streaming.
     */
    static constexpr std::size_t    MAX_ORB_CAPACITY = 256000;          ///< Maximum GPU capacity for orb creature instance buffer.
    OrbSSBO                         m_orbSSBO{MAX_ORB_CAPACITY};       ///< SSBO manager for orb instances.
    std::vector<OrbData>            m_orbStagingBuffer;                 ///< CPU staging vector for packing active orb instances.


    // =========================================================================
    // Rendering — HUD
    // =========================================================================

    std::unique_ptr<HUD> m_hud;     ///< User interface head-up display manager pointer.
    HUD_Data             m_hudData; ///< UI view model data binding container.

    // =========================================================================
    // Player and Camera Feel
    // =========================================================================

    sf::Vector3f m_cameraBobOffset{0.f, 0.f, 0.f}; ///< Offset vector applied for camera view bobbing.
    float m_bobLag                  = 0.16f;  ///< Smoothing interpolation factor for camera bobbing $[0, 1]$.
    float m_crouchFactor            = 0.0f;   ///< Crouch interpolation state scalar $[0 = \text{standing}, 1 = \text{crouched}]$.

    HeadlightState m_headlightState = HeadlightState::Auto; ///< Selected headlight activation behavior state.

    // =========================================================================
    // Debug and Editor Flags
    // =========================================================================

    bool m_drawGrid                 = true;     ///< Toggle visibility of coordinate reference grid.
    bool m_drawHexGrid              = false;    ///< Toggle visibility of tactical hex cell grid lines.
    bool m_drawTextures             = true;     ///< Toggle surface texture sampling vs untextured debug shading.
    bool m_drawCollision            = false;    ///< Toggle rendering of entity collision boundaries.
    bool m_showGUI                  = false;    ///< Toggle rendering of ImGui diagnostic overlay windows.
    bool m_cursorMode               = false;    ///< Toggle active mouse picking cursor vs locked camera look.
    glm::vec3 m_nightAmbientFloor   = glm::vec3(0.002f, 0.003f, 0.006f); ///< RGB floor color for ambient night lighting.
    bool m_debugShowSSAO            = false;    ///< Debug flag to display unblurred SSAO texture on screen.
    bool m_debugShowSSAOBlur        = false;    ///< Debug flag to display blurred SSAO texture on screen.
    float m_debugSSAOKernelRadius   = 0.25f;    ///< World-space radius for SSAO sampling hemisphere.
    float m_debugSSAOBias           = 0.005f;   ///< Depth bias threshold offset preventing SSAO self-occlusion.
    int m_sampleCount               = 64;       ///< Number of kernel samples used in SSAO calculation.
    int m_ssaoKernelSize            = 64;       ///< Size of SSAO sample pattern array.
    float m_orbUpdateMs             = 0.0f;     ///< Milliseconds spent updating orb SSBO data per frame.
    float m_orbGatherMS             = 0.0f;     ///< Milliseconds spent gathering orb ECS data per frame.
    float m_orbUploadMS             = 0.0f;     ///< Milliseconds spent uploading orb SSBO data per frame.

    // =========================================================================
    // Performance Tracking
    // =========================================================================

    sf::Clock m_fpsClock;            ///< High-precision timer clock for measuring frame rate.
    int   m_fpsFrameCount           = 0;///< Frame accumulator counter.
    float m_fps                     = 0.0f;///< Evaluated frames-per-second metric.
    float m_lastFrameTime           = 0.0f;///< Delta time duration of previous frame in seconds.

    // =========================================================================
    // Scene Initialization
    // =========================================================================

    /** @brief Loads level manifest and entity data from specified file path. */
    void loadLevel(const std::string& filename);
    
    /** @brief Instantiates player character entity in ECS world. */
    void spawnPlayer();

    /** @brief Instantiates view camera entity in ECS world. */
    void spawnCamera();

    /**
     * @brief Spawns an orb creature entity batch within specified hex coordinate bounds.
     * @param hexQ Axial Q hex coordinate center.
     * @param hexR Axial R hex coordinate center.
     * @param radius Dispersion radius around hex center.
     * @param bobRate Vertical bob oscillation frequency.
     * @param bobMagnitude Vertical bob oscillation magnitude.
     * @param eyes Eye component attributes for creature batch.
     * @param yaw Yaw rotation angle in degrees.
     * @param species Ocular species identifier mapping into `SpeciesSSBO`.
     */
    void spawnOrbFauna(int hexQ, int hexR, float radius,
                      float bobRate = 2.0f, float bobMagnitude = 8.0f,
                      const CEyes& eyes = CEyes(), float yaw = 0.0f, int species = 0);

    /** @brief Spawns stress-test batch of debug orb entities. */
    void spawnDebugOrbs(int count);

    /** @brief Constructs HUD layout components. */
    void buildHud();

    /** @brief Builds terrain grid rendering buffers and geometry layout. */
    void buildTerrainGrid();

    /** @brief Builds unit vertex cube geometry for sphere impostors. */
    void buildVertexCube();

    /**
     * @brief Generates procedural ocean surface grid mesh geometry.
     * @param size Grid side length in world space units.
     * @param resolution Grid division resolution count.
     * @param[out] vao Destination VAO handle.
     * @param[out] vbo Destination VBO handle.
     * @param[out] ebo Destination EBO handle.
     * @param[out] indexCount Destination total element index count.
     */
    void generateOceanMesh(float size, int resolution, unsigned int& vao, unsigned int& vbo, unsigned int& ebo, unsigned int& indexCount);

    /** @brief Loads and uploads sky cubemap textures to OpenGL context. */
    void initializeSkyCubemap();

    /** @brief Initializes Orb SSBO backing storage allocations. */
    void initializeOrbShaderStorage();

    /** @brief Allocates G-Buffer render textures and framebuffer attachments. */
    void initializeGBuffer(unsigned int width, unsigned int height);

    /** @brief Creates 4x4 noise texture containing random rotation vectors for SSAO. */
    void initSSAONoiseTexture();

    /** @brief Generates hemispherical sample kernel array for SSAO pass. */
    void initSSAOKernel();

    /** @brief Allocates SSAO framebuffer objects and depth-stencil texture attachments. */
    void initSSAOFramebuffers();

    /** @brief Destroys and cleans up allocated G-Buffer textures and framebuffer. */
    void destroyGBuffer();

    /** @brief Allocates 2D Texture Array and framebuffer for Cascaded Shadow Mapping. */
    void initializeShadowMap(unsigned int size);

    /** @brief Releases shadow mapping textures and framebuffer handles. */
    void destroyShadowMap();

    /** @brief Constructs Uniform Buffer Objects (UBOs) for camera and environment data blocks. */
    void initUBOs();

    /** @brief Initializes scene state configuration, time settings, and camera vectors. */
    void initSceneConfiguration();

    /** @brief Compiles, links, and validates all scene OpenGL shader pipelines. */
    void initGraphicsPipelines();

    /** @brief Initializes current level state parameters. */
    void initLevelState();

    /** @brief Cleans up and releases all scene OpenGL graphic resources. */
    void cleanUpGraphicsResources();

    // =========================================================================
    // Per-Frame Updates
    // =========================================================================

    /** @brief Updates camera position, rotation matrices, and bobbing offsets. */
    void updateCamera(float dt);

    /** @brief Refreshes HUD UI data bindings from active game state. */
    void updateHUDData();

    /** @brief Renders and updates minimap radar texture. */
    void updateMinimapTexture();

    /** @brief Recalculates astronomical celestial positions and sky lighting vectors. */
    void updateAstronomySystem();

    /** @brief Updates movement head-bobbing animation offsets for entity. */
    void updateBob(SoAEntityHandle e, float dt, float horizSpeed);

    /** @brief Updates vertical bobbing oscillation for orb fauna entity. */
    void updateOrbBobbing(SoAEntityHandle e, float dt);

    /**
     * @brief Computes Light View-Projection matrix for a specific camera frustum slice.
     * @param splitNear Near boundary of current cascade split.
     * @param splitFar Far boundary of current cascade split.
     * @param[out] lightDepthRange Output depth span range for orthographic projection.
     * @param[out] texelWorldSize Output texel side length in world space.
     * @return 4x4 Light View-Projection matrix.
     */
    glm::mat4 computeLightViewProjForRange(float splitNear, float splitFar, float& lightDepthRange, float& texelWorldSize) const;

    /** @brief Computes bounding light matrix for full world map extent. */
    glm::mat4 computeLightViewProjForMapBounds(float& lightDepthRange, float& texelWorldSize) const;

    /** @brief Calculates maximum distance from camera to furthest visible world corner. */
    float computeDistanceToMapFarCorner(const glm::vec3& cameraPos) const;

    /** @brief Calculates split distance thresholds for cascading shadow maps (CSM). */
    void computeCascadeSplits(float camNear, float camFar);

    /** @brief Uploads view frustum corner rays to specified shader program. */
    void uploadViewRays(GLuint shaderProgram);

    // =========================================================================
    // Movement and Physics
    // =========================================================================

    /** @brief Evaluates entity physics, movement vectors, and velocity updates. */
    void sMovement(float dt);

    /** @brief Processes player input vectors to handle movement and rotation. */
    void handlePlayerMovement(SoAEntityHandle player, float dt);

    /** @brief Evaluates player gait state and triggers footstep audio events. */
    void sGaitAndFootsteps(float dt);

    /** @brief Resolves entity terrain collision and ground snapping. */
    void resolveEntityPosition(SoAEntityHandle e, float dt);

    /** @brief Evaluates environmental conditions to determine if player headlights should activate automatically. */
    bool shouldHeadlightsBeOn() const;

    // =========================================================================
    // Render Passes
    // =========================================================================

    /** @brief Renders depth maps for active CSM cascades into shadow texture array. */
    void runShadowPass();

    /** @brief Executes main terrain geometry deferred rendering pass into G-Buffer. */
    void runTerrainPass();

    /** @brief Renders instanced orb creatures using sphere impostor shader into G-Buffer. */
    void renderOrbCreature();

    /** @brief Renders celestial skybox, star field, sun, and moon dome. */
    void renderSky();

    /** @brief Executes Screen-Space Ambient Occlusion (SSAO) evaluation pass. */
    void runSSAOPass();

    /** @brief Executes spatial bilateral blur on computed SSAO buffer. */
    void runSSAOBlurPass();

    /** @brief Performs deferred lighting accumulation pass combining G-Buffer textures, shadows, and SSAO. */
    void deferredLighting();

    /** @brief Blits final scene render target texture to default screen framebuffer. */
    void blitToScreen(GLuint tex);

    /** @brief Renders ocean mesh grid with procedural dynamic wave displacement. */
    void renderOceanGrid();

    // =========================================================================
    // Shader Uniform Upload
    // =========================================================================

    /** @brief Streams current frame active orb instance data into GPU SSBO buffer. */
    void updateOrbShaderStorage();

    // =========================================================================
    // Terrain Query Helpers
    // =========================================================================

    /**
     * @brief Measures camera height distance above terrain surface.
     * @param cameraPos 3D camera location in world space.
     * @return Distance scalar in meters above ground level.
     */
    float getCameraHeightAboveGround(const sf::Vector3f& cameraPos) const;

    /**
     * @brief Queries terrain height elevation at world XZ coordinates.
     * @param x World space X position coordinate.
     * @param z World space Z position coordinate.
     * @return Elevation height in meters.
     */
    float heightAt(float x, float z) const {
        return Topography::heightAt(getTerrainContext(), sf::Vector2f(x, z));
    }

    /**
     * @brief Queries terrain surface normal vector at world XZ coordinates.
     * @param x World space X position coordinate.
     * @param z World space Z position coordinate.
     * @return Normalized 3D surface normal vector.
     */
    sf::Vector3f normalAt(float x, float z) const {
        return Topography::normalAt(getTerrainContext(), sf::Vector2f(x, z));
    }

    // =========================================================================
    // Coordinate and Color Utilities
    // =========================================================================

    /**
     * @brief Converts SFML color object to GLSL vector 3 normalized format.
     * @param color Input SFML color.
     * @return Normalized RGB vector in range $[0.0, 1.0]$.
     */
    sf::Glsl::Vec3 colorToShader(const sf::Color& color);

    /**
     * @brief Unprojects screen mouse pixel location into 3D world space position.
     * @param mousePos 2D screen pixel location.
     * @return Reconstructed 3D world position vector.
     */
    sf::Vector3f   screenToWorld(sf::Vector2i mousePos) const;

public:

    // =========================================================================
    // Public Interface
    // =========================================================================

    /**
     * @brief Constructs `Scene_IC_Camp` instance.
     * @param game Reference to primary `GameEngine` host.
     * @param levelPath File path to level manifest file.
     */
    Scene_IC_Camp(GameEngine& game, const std::string& levelPath);

    /**
     * @brief Destructor releasing allocated graphics resources and scene state.
     */
    ~Scene_IC_Camp() override;

    /** @brief Renders ImGui diagnostic overlays, performance counters, and tweak menus. */
    void sGUI();

    /** @brief Main scene update tick handler called once per frame. */
    void update() override;

    /** @brief Handles user input actions dispatched by primary game loop. */
    void sDoAction(const Action& action) override;

    /** @brief Primary render invocation executing scene pipelines in order. */
    void sRender() override;

    /** @brief Lifecycle callback invoked when scene gains active focus. */
    void onEnter() override;

    /** @brief Lifecycle callback invoked when scene loses active focus. */
    void onExit() override;

    /** @brief Lifecycle callback invoked when scene terminates. */
    void onEnd() override;

    /**
     * @brief Gets pointer to HUD user interface controller.
     * @return Pointer to active `HUD` instance.
     */
    HUD* getHUD() const override;

    /**
     * @brief Constructs `Topography::TerrainContext` bound to active terrain streamer.
     * @return Populated `TerrainContext` structure.
     */
    Topography::TerrainContext getTerrainContext() const;
};