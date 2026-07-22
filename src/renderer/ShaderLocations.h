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

    // =========================================================================
    // PER-PROGRAM UNIFORM BLOCKS (Starts at 0)
    // =========================================================================

    namespace Ocean {
        constexpr GLint Model              = 0;
        constexpr GLint NormalMatrix       = 1;
        constexpr GLint Time               = 2;
        constexpr GLint GDepth             = 3;
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

} // namespace Uniforms