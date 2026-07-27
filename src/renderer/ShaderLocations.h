#pragma once
#include <GL/glew.h>

namespace Uniforms {

    // =========================================================================
    // SHARED / GLOBAL UNIFORM BLOCKS (Reserved Ranges)
    // =========================================================================
    
    namespace Sky {
        constexpr GLint Base                    = 100;
        constexpr GLint Cubemap                 = Base + 0;
        constexpr GLint StarRotationMatrix      = Base + 1;
        constexpr GLint UseSkyCubemap           = Base + 2;
        constexpr GLint MoonTexture             = Base + 3;
    }

    namespace Shadows {
        constexpr GLint Base                    = 200;
        constexpr GLint LightViewProj           = Base + 0;
        constexpr GLint TexelWorldSize          = Base + 5;
        constexpr GLint CascadeSplitDepths      = Base + 10;
        constexpr GLint ShadowMapArray          = Base + 15;
    }

    namespace GBuffer {
        constexpr GLint Base                    = 300;
        constexpr GLint GAlbedoTex              = Base + 0;
        constexpr GLint GNormalTex              = Base + 1;
        constexpr GLint GIndicesTex             = Base + 2;
        constexpr GLint GRetroTex               = Base + 3;
        constexpr GLint GDepthTex               = Base + 4;
    }

    namespace SharedTerrain {
        constexpr GLint Base                    = 400;
        constexpr GLint HeightMax               = Base + 0;
        constexpr GLint TerrainGridWorldOrigin  = Base + 1;
        constexpr GLint TerrainTileWorldSize    = Base + 2;
        constexpr GLint TerrainHeightArray      = Base + 3;
        constexpr GLint TerrainRoadArray        = Base + 4;
        constexpr GLint TerrainSliceValid       = Base + 5;
    }

    // =========================================================================
    // PER-PROGRAM UNIFORM BLOCKS (Start at 0)
    // =========================================================================

    namespace Ocean {
        constexpr GLint Model                   = 0;
        constexpr GLint NormalMatrix            = 1;
        constexpr GLint Time                    = 2;
        constexpr GLint NightAmbientFloor       = 3;
    }

    namespace Terrain {
        constexpr GLint ReliefExaggeration      = 0;
        constexpr GLint CursorMode              = 1;
        constexpr GLint HexSize                 = 2;
        constexpr GLint HoveredHex              = 3;
        constexpr GLint GridColour              = 4;
        constexpr GLint SeaLevel                = 5;
        constexpr GLint TerrainDiffuseArray     = 6;
        constexpr GLint TerrainNormalArray      = 7;
    }

    namespace Lighting {
        constexpr GLint NightAmbientFloor       = 0;
        constexpr GLint HeadlampIntensity       = 1;
        constexpr GLint HeadlampRange           = 2;
        constexpr GLint HeadlampEnabled         = 3;
        constexpr GLint SSAOTex                 = 4;
    }

    namespace SSAO {
        constexpr GLint NoiseTex                = 0;
        constexpr GLint Radius                  = 1;
        constexpr GLint Bias                    = 2;
        constexpr GLint NoiseScale              = 3;
        constexpr GLint SampleCount             = 4;
        constexpr GLint TopRight                = 5;
        constexpr GLint TopLeft                 = 6;
        constexpr GLint BottomLeft              = 7;
        constexpr GLint BottomRight             = 8;
        constexpr GLint KernelSample            = 9;
    }

    namespace SSAOBlur {
        constexpr GLint SSAOInput               = 0;
    }

    namespace MiniMap {
        constexpr GLint PlayerXZ                = 0;
        constexpr GLint WorldRadius             = 1;
        constexpr GLint SeaLevel                = 2;
    }

    namespace ShadowPass {
        constexpr GLint LightViewProj           = 0;
    }

    namespace OrbCreature {
        constexpr GLint DiffuseTex              = 0;
        constexpr GLint NormalTex               = 1;
    }

    namespace Blit {
        constexpr GLint InputTex                = 0;
    }

} // namespace Uniforms