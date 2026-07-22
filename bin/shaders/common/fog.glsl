#ifndef FOG_GLSL
#define FOG_GLSL

// ------------------------------------------------------------------
// 1. Geometry Pass: Finite Ray Length
// ------------------------------------------------------------------
float computeHeightFogAmount(
    vec3 rayDir, 
    float rayLen, 
    float cameraHeightAboveBase, 
    float density, 
    float heightFalloff
) {
    float verticalTerm;
    
    // Prevent division-by-zero for nearly horizontal rays
    if (abs(rayDir.y) > 0.001) {
        verticalTerm = (1.0 - exp(-rayLen * rayDir.y * heightFalloff)) / (rayDir.y * heightFalloff);
    } else {
        verticalTerm = rayLen;
    }

    float fogAmount = density * exp(-cameraHeightAboveBase * heightFalloff) * verticalTerm;
    return clamp(fogAmount, 0.0, 1.0);
}

// ------------------------------------------------------------------
// 2. Sky Pass: Infinite Ray Length (rayLen -> infinity)
// ------------------------------------------------------------------
float computeHeightFogAmount(
    vec3 rayDir, 
    float cameraHeightAboveBase, 
    float density, 
    float heightFalloff
) {
    float fogAmount;

    if (rayDir.y > 0.001) {
        float verticalTerm = 1.0 / (rayDir.y * heightFalloff);
        fogAmount = density * exp(-cameraHeightAboveBase * heightFalloff) * verticalTerm;
    } else {
        // Looking horizontal or below horizon into infinite fog
        fogAmount = 1.0;
    }

    return clamp(fogAmount, 0.0, 1.0);
}

#endif // FOG_GLSL