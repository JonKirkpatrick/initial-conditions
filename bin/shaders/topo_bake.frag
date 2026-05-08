uniform vec2 viewportSize;
uniform vec3 cameraPos;
uniform float farPlane;
uniform float nearPlane;
uniform float fovY;
uniform float aspectRatio;
uniform mat3 invRotationMatrix;
uniform float u_quality; // 0.05..1.0, scales raymarch cost
uniform float u_stepSizeScale; // multiplier for base step size (default 1.0)
uniform float u_activeLayerEnabled[16]; // 1.0 when layer i is enabled, 0.0 otherwise
uniform float terrainFloorY; // analytic cutoff plane below terrain
uniform float terrainCeilingY; // analytic cutoff plane above terrain

// Terrain layer parameters (16 layers)
uniform vec2 layer_center[16];
uniform float layer_radius[16];
uniform float layer_falloffWidth[16];
uniform float layer_topoScale[16];
uniform float layer_frequency[16];
uniform float layer_boundaryHeight[16];

float maskFromD(float d, float rd, float falloff) {
    float t = falloff;           // falloff distance

    // Inside the circle: full strength
    float inside = step(d, 0.0);  // 1.0 if d <= 0, else 0.0

    // Smooth transition zone using cosine
    float u = t - abs(d - t);
    float g = clamp(0.5 * (1.0 + u / (abs(u) - 1e-10)), 0.0, 1.0);

    float cosTerm = cos(3.141592653589793 * d / (2.0 * t));
    float b = g * ((cosTerm + 1.0) * 0.5);

    return inside + b;
}

float evaluateLayerHeightAt(in vec2 xz, int layerIdx) {
    vec2 delta = xz - layer_center[layerIdx];
    float distSq = dot(delta, delta);
    
    float radius = layer_radius[layerIdx];
    
    float distFromCenter = sqrt(distSq);
    float d = distFromCenter - radius;
    float falloff = layer_falloffWidth[layerIdx];
    
    // Use your sophisticated mask
    float mask = maskFromD(d, radius, falloff);
    
    // Topography simple
    float topo = 500.0;
    
    float height = topo * mask;
    
    return height;
}

float heightAt(vec2 xz) {
    float height = 0.0;
    
    for (int i = 0; i < 16; ++i) {
        if (u_activeLayerEnabled[i] > 0.5) {
            height += evaluateLayerHeightAt(xz, i);
        }
    }
    return height;
}

void main() {
    vec2 screen = gl_FragCoord.xy;
    screen.y = viewportSize.y - screen.y;

    float x_ndc = (screen.x / viewportSize.x) * 2.0 - 1.0;
    float y_ndc = 1.0 - (screen.y / viewportSize.y) * 2.0;

    float f = tan(fovY * 0.5);
    vec3 rayDir = normalize(invRotationMatrix * vec3(x_ndc * f * aspectRatio, y_ndc * f, -1.0));

    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0); // sky

    rayDir.y = abs(rayDir.y) < 0.000001 ? sign(rayDir.y + 0.000001) * 0.000001 : rayDir.y;

    float t = 0.0;
    float tPrev = 0.0;
    bool isHit = false;

    // Adaptive step count: very shallow rays (horizon) give up early to avoid crawl
    float rayShallowness = abs(rayDir.y);
    float q = clamp(u_quality, 0.05, 1.0);

    // Analytic cutoff plane bounds the march for sky-heavy views.
    float maxTravel = farPlane;

    float baseMaxF = rayShallowness < 0.05 ? 300.0 : (rayShallowness < 0.15 ? 600.0 : 1000.0);
    float maxStepsF = max(50.0, baseMaxF * q);
    int maxSteps = int(maxStepsF);

    for (int i = 0; i < 500; ++i) {
        if (i >= maxSteps) break;

        vec3 p = cameraPos + rayDir * t;
        if (t > maxTravel) break;

        float h = heightAt(p.xz);
        float distToSurf = p.y - h;

        if (distToSurf < 0.0) {
            // Binary refinement
            float t_low  = tPrev;
            float t_high = t;
            float baseRefineF = rayShallowness < 0.12 ? 12.0 : 8.0;
            int refineSteps = int(max(3.0, baseRefineF * q));
            for (int j = 0; j < 12; ++j) {
                if (j >= refineSteps) break;
                float t_mid = mix(t_low, t_high, 0.5);
                vec3 p_mid = cameraPos + rayDir * t_mid;
                if (p_mid.y < heightAt(p_mid.xz)) {
                    t_high = t_mid;
                } else {
                    t_low = t_mid;
                }
            }
            t = mix(t_low, t_high, 0.5);
            isHit = true;
            break;
        }

        // Convert vertical clearance to along-ray travel.
        // Near the horizon (small |rayDir.y|), this avoids tiny progress per step.
        float minAngle = 0.12 + (1.0 - q) * 0.2; // lower quality -> larger angle floor
        float angleScale = max(rayShallowness, minAngle);
        float stepSize = max(10.0 + (1.0 - q) * 30.0, (distToSurf / angleScale) * 0.55);
        float ss = clamp(u_stepSizeScale, 0.1, 5.0);
        stepSize = clamp(stepSize * ss, 1.0 * ss, 220.0 * ss);
        tPrev = t;
        t += stepSize;
    }

    if (!isHit) {
        // === Fallback to flat ground plane at y = 0 ===
        if (rayDir.y < -0.00001) {
            float tGround = -cameraPos.y / rayDir.y;
            
            // Accept ground hit if it's within reasonable bounds
            if (tGround > nearPlane * 0.5 && tGround < maxTravel * 1.2) {
                t = tGround;
                isHit = true;
            }
        }
    }

    if (!isHit) {
        return; // sky
    }

    float normalized = clamp((t - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
    float r = floor(normalized * 255.0) / 255.0;
    float g = fract(normalized * 255.0);
    float b = fract(normalized * 255.0 * 255.0);

    gl_FragColor = vec4(r, g, b, 1.0);
}