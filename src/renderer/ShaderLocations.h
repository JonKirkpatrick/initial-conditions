#pragma once
#include <GL/glew.h>

namespace Uniforms {

    // =========================================================================
    // SHARED / GLOBAL UNIFORM BLOCKS (Reserved Ranges)
    // =========================================================================
    
    namespace Sky {
        constexpr GLint Base               = 100;
        constexpr GLint Cubemap            = Base + 0;
        constexpr GLint StarRotationMatrix = Base + 1;
        constexpr GLint UseSkyCubemap      = Base + 2;
        constexpr GLint MoonTexture        = Base + 3;
    }

    namespace Shadows {
        constexpr GLint Base               = 200;
        constexpr GLint LightViewProj      = Base + 0;
        constexpr GLint TexelWorldSize     = Base + 5;
        constexpr GLint CascadeSplitDepths = Base + 10;
        constexpr GLint ShadowMapArray     = Base + 15;
    }

    namespace GBuffer {
        constexpr GLint Base               = 300;
        constexpr GLint GAlbedoTex         = Base + 0;
        constexpr GLint GNormalTex         = Base + 1;
        constexpr GLint GIndicesTex        = Base + 2;
        constexpr GLint GRetroTex          = Base + 3;
        constexpr GLint GDepthTex          = Base + 4;
    }

    // =========================================================================
    // PER-PROGRAM UNIFORM BLOCKS (Start at 0)
    // =========================================================================

    namespace Ocean {
        constexpr GLint Model              = 0;
        constexpr GLint NormalMatrix       = 1;
        constexpr GLint Time               = 2;
        constexpr GLint NightAmbientFloor  = 4;
    }

    namespace Terrain {
        constexpr GLint HeightMax               = 0;
        constexpr GLint ReliefExaggeration      = 1;
        constexpr GLint CursorMode              = 2;
        constexpr GLint HexSize                 = 3;
        constexpr GLint HoveredHex              = 4;
        constexpr GLint GridColour              = 5;
        constexpr GLint TerrainGridWorldOrigin  = 6;
        constexpr GLint TerrainTileWorldSize    = 7;
        constexpr GLint TerrainHeightArray      = 8;
        constexpr GLint TerrainSliceValid       = 9;
    }

    namespace TerrainShadow {}

    namespace Lighting {
        constexpr GLint NightAmbientFloor       = 0;
        constexpr GLint HeadlampIntensity       = 1;
        constexpr GLint HeadlampRange           = 2;
        constexpr GLint HeadlampEnabled         = 3;
        constexpr GLint SSAOTex                 = 4;
    }

    namespace SSAO {
        constexpr GLint NoiseTex               = 0;
        constexpr GLint Radius                 = 1;
        constexpr GLint Bias                   = 2;
        constexpr GLint NoiseScale             = 3;
        constexpr GLint SampleCount            = 4;
        constexpr GLint KernelSample           = 5;
    }

} // namespace Uniforms