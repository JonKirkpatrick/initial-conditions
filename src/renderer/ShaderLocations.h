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

    namespace Fog {
        constexpr GLint Base               = 120;
        constexpr GLint ColorDay           = Base + 0;
        constexpr GLint ColorNight         = Base + 1;
        constexpr GLint Density            = Base + 2;
        // ...
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

} // namespace Uniforms