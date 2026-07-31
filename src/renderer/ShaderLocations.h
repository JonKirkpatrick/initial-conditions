/**
 * @file UniformLocations.hpp
 * @brief Static uniform location index registry for OpenGL shader programs.
 * 
 * Defines explicit uniform locations mapping to layout location bindings (`layout(location = N)`).
 * Provides distinct index ranges for shared global properties (Sky, Shadows, GBuffer, Terrain)
 * and program-specific uniforms (Ocean, Terrain, Lighting, SSAO, MiniMap, etc.).
 * 
 * @note Planned migration: This static header will be replaced by a JSON-driven runtime schema 
 * generated during `GLProgram` compilation to serve as a single source of truth.
 */

#pragma once
#include <GL/glew.h>

namespace Uniforms {

    // =========================================================================
    // SHARED / GLOBAL UNIFORM BLOCKS (Reserved Ranges)
    // =========================================================================
    
    /**
     * @brief Uniform layout locations for skybox, celestial bodies, and star field rendering [Range 100-199].
     */
    namespace Sky {
        constexpr GLint Base                    = 100;    ///< Base location offset for sky uniforms.
        constexpr GLint Cubemap                 = Base + 0; ///< Texture sampler for sky background cubemap.
        constexpr GLint StarRotationMatrix      = Base + 1; ///< 3x3 rotation matrix for nocturnal star field rotation.
        constexpr GLint UseSkyCubemap           = Base + 2; ///< Boolean flag toggling sky cubemap sampling.
        constexpr GLint MoonTexture             = Base + 3; ///< 2D texture sampler for dynamic moon rendering.
    }

    /**
     * @brief Uniform layout locations for directional cascaded shadow maps (CSM) [Range 200-299].
     */
    namespace Shadows {
        constexpr GLint Base                    = 200;    ///< Base location offset for shadow mapping uniforms.
        constexpr GLint LightViewProj           = Base + 0; ///< Array of View-Projection matrices for shadow cascade splits.
        constexpr GLint TexelWorldSize          = Base + 5; ///< Shadow map texel dimensions in world units.
        constexpr GLint CascadeSplitDepths      = Base + 10;///< View-space split distance thresholds for CSM cascades.
        constexpr GLint ShadowMapArray          = Base + 15;///< 2D Array texture sampler (`GL_TEXTURE_2D_ARRAY`) for CSM depths.
    }

    /**
     * @brief Uniform layout locations for Deferred Rendering G-Buffer samplers [Range 300-399].
     */
    namespace GBuffer {
        constexpr GLint Base                    = 300;    ///< Base location offset for G-Buffer texture uniforms.
        constexpr GLint GAlbedoTex              = Base + 0; ///< G-Buffer albedo/specular 2D texture sampler.
        constexpr GLint GNormalTex              = Base + 1; ///< G-Buffer world normal 2D texture sampler.
        constexpr GLint GIndicesTex             = Base + 2; ///< G-Buffer material index / ID 2D texture sampler.
        constexpr GLint GRetroTex               = Base + 3; ///< G-Buffer retro-reflective / custom channel 2D sampler.
        constexpr GLint GDepthTex               = Base + 4; ///< Deferred scene depth buffer 2D texture sampler.
    }

    /**
     * @brief Uniform layout locations for shared streaming terrain texture arrays [Range 400-499].
     */
    namespace SharedTerrain {
        constexpr GLint Base                    = 400;    ///< Base location offset for terrain streaming uniforms.
        constexpr GLint HeightMax               = Base + 0; ///< Maximum height scale threshold in world units.
        constexpr GLint TerrainGridWorldOrigin  = Base + 1; ///< 2D world space origin coordinate of visible grid.
        constexpr GLint TerrainTileWorldSize    = Base + 2; ///< Side length dimension of individual terrain tile in world units.
        constexpr GLint TerrainHeightArray      = Base + 3; ///< 2D texture array handle containing terrain height slices.
        constexpr GLint TerrainRoadArray        = Base + 4; ///< 2D texture array handle containing road SDF distance slices.
        constexpr GLint TerrainSliceValid       = Base + 5; ///< Uniform array of boolean validity flags for active grid slices.
    }

    // =========================================================================
    // PER-PROGRAM UNIFORM BLOCKS (Start at 0)
    // =========================================================================

    /**
     * @brief Program-specific uniform locations for ocean surface shader.
     */
    namespace Ocean {
        constexpr GLint Model                   = 0; ///< 4x4 Model matrix for ocean mesh.
        constexpr GLint NormalMatrix            = 1; ///< 3x3 Normal matrix for ocean mesh vectors.
        constexpr GLint Time                    = 2; ///< Total elapsed time in seconds for animated wave synthesis.
        constexpr GLint NightAmbientFloor       = 3; ///< Minimum lighting floor intensity during nighttime.
    }

    /**
     * @brief Program-specific uniform locations for terrain surface shader.
     */
    namespace Terrain {
        constexpr GLint ReliefExaggeration      = 0; ///< Vertical elevation multiplier for terrain exaggeration.
        constexpr GLint CursorMode              = 1; ///< Active terrain editing or interaction cursor mode ID.
        constexpr GLint HexSize                 = 2; ///< Dimension radius of tactical hex grid overlay cells.
        constexpr GLint HoveredHex              = 3; ///< 2D coordinate of currently highlighted/hovered hex cell.
        constexpr GLint GridColour              = 4; ///< RGBA color for rendering tactical hex grid lines.
        constexpr GLint SeaLevel                = 5; ///< World elevation threshold representing sea level altitude.
        constexpr GLint TerrainDiffuseArray     = 6; ///< 2D Array texture sampler for terrain albedo materials.
        constexpr GLint TerrainNormalArray      = 7; ///< 2D Array texture sampler for terrain normal maps.
        constexpr GLint DrawHexGrid             = 8; ///< Boolean flag toggling hex grid overlay rendering.
    }

    /**
     * @brief Program-specific uniform locations for deferred lighting pass shader.
     */
    namespace Lighting {
        constexpr GLint NightAmbientFloor       = 0; ///< Minimum ambient illumination factor during night cycle.
        constexpr GLint HeadlampIntensity       = 1; ///< Brightness scalar for player headlamp light source.
        constexpr GLint HeadlampRange           = 2; ///< Effective illumination distance for player headlamp.
        constexpr GLint HeadlampEnabled         = 3; ///< Boolean flag toggling player headlamp activation.
        constexpr GLint SSAOTex                 = 4; ///< 2D texture sampler for Screen-Space Ambient Occlusion map.
    }

    /**
     * @brief Program-specific uniform locations for Screen-Space Ambient Occlusion (SSAO) pass.
     */
    namespace SSAO {
        constexpr GLint NoiseTex                = 0; ///< 2D texture sampler containing randomized rotation vectors.
        constexpr GLint Radius                  = 1; ///< Sampling hemisphere radius in view space units.
        constexpr GLint Bias                    = 2; ///< Depth bias offset preventing self-occlusion artifacts.
        constexpr GLint NoiseScale              = 3; ///< Tiling scale factor mapping noise texture across viewport.
        constexpr GLint SampleCount             = 4; ///< Total number of kernel sample points evaluated per fragment.
        constexpr GLint TopRight                = 5; ///< Top-right frustum corner vector for view-ray reconstruction.
        constexpr GLint TopLeft                 = 6; ///< Top-left frustum corner vector for view-ray reconstruction.
        constexpr GLint BottomLeft              = 7; ///< Bottom-left frustum corner vector for view-ray reconstruction.
        constexpr GLint BottomRight             = 8; ///< Bottom-right frustum corner vector for view-ray reconstruction.
        constexpr GLint KernelSample            = 9; ///< Array of 3D sample vectors distributed within hemisphere.
    }

    /**
     * @brief Program-specific uniform locations for SSAO post-process spatial blur pass.
     */
    namespace SSAOBlur {
        constexpr GLint SSAOInput               = 0; ///< 2D raw SSAO texture input to be blurred.
    }

    /**
     * @brief Program-specific uniform locations for tactical mini-map rendering pass.
     */
    namespace MiniMap {
        constexpr GLint PlayerXZ                = 0; ///< 2D world XZ coordinates of player location.
        constexpr GLint WorldRadius             = 1; ///< Total visible radius represented on mini-map display.
        constexpr GLint SeaLevel                = 2; ///< World altitude threshold for rendering water features.
    }

    /**
     * @brief Program-specific uniform locations for depth-only shadow map generation pass.
     */
    namespace ShadowPass {
        constexpr GLint LightViewProj           = 0; ///< 4x4 Light View-Projection matrix for current CSM cascade.
    }

    /**
     * @brief Program-specific uniform locations for creature orb surface shader.
     */
    namespace OrbCreature {
        constexpr GLint DiffuseTex              = 0; ///< 2D diffuse albedo texture sampler for orb skin.
        constexpr GLint NormalTex               = 1; ///< 2D normal map texture sampler for orb skin.
    }

    /**
     * @brief Program-specific uniform locations for full-screen texture blitting pass.
     */
    namespace Blit {
        constexpr GLint InputTex                = 0; ///< 2D input texture to copy to render target.
    }

} // namespace Uniforms