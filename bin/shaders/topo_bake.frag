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
uniform float u_heightmapTransitionThreshold; // distance above cached heightmap before raymarching
uniform sampler2D topoTopdownTex;
uniform vec2 topdownWorldMin;
uniform vec2 topdownWorldSize;
uniform float topdownHeightMax;

// Terrain layer parameters (16 layers)
uniform vec2 layer_center[16];
uniform float layer_radius[16];
uniform float layer_falloffWidth[16];
uniform float layer_topoHeight[16];

#include "topo_common.glsl"

float decodePackedHeight(vec4 rgba, float maxHeightValue)
{
    vec3 bytes = floor(rgba.rgb * 255.0 + 0.5);

    float scaled = dot(
        bytes,
        vec3(65536.0, 256.0, 1.0)
    );

    return scaled * (maxHeightValue / 16777215.0);
}

bool sampleTopdownHeight(in vec2 xz, out float outH) {
    vec2 uv = (xz - topdownWorldMin) / topdownWorldSize;
    uv.y = 1.0 - uv.y;

    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return false;
    }

    vec4 c = texture2D(topoTopdownTex, uv);
    outH = decodePackedHeight(c, topdownHeightMax);
    return true;
}

vec4 encodeU24(float normalized)
{
    normalized = clamp(normalized, 0.0, 1.0);

    normalized = min(normalized, 0.99999994);

    vec3 enc = fract(normalized * vec3(1.0, 256.0, 65536.0));

    enc -= enc.yzz * vec3(
        1.0 / 256.0,
        1.0 / 256.0,
        0.0
    );

    return vec4(enc, 1.0);
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
    float prevDistApprox = -1e20; // sentinel for "no previous"
    float prevStep = 0.0;

    if (rayDir.y > 0.0 && cameraPos.y >= topdownHeightMax) {
        return; // upward ray already starts above the tallest terrain
    }

    // Adaptive step count: very shallow rays (horizon) give up early to avoid crawl
    float rayShallowness = abs(rayDir.y);
    float q = clamp(u_quality, 0.05, 1.0);

    // Analytic cutoff plane bounds the march for sky-heavy views.
    float maxTravel = farPlane;

    float baseMaxF = rayShallowness < 0.05 ? 300.0 : (rayShallowness < 0.15 ? 600.0 : 1000.0);
    float maxStepsF = max(50.0, baseMaxF * q);
    int maxSteps = int(maxStepsF);

    for (int i = 0; i < maxSteps; ++i) {
        vec3 p = cameraPos + rayDir * t;
        if (t > maxTravel) break;
        if (rayDir.y > 0.0 && p.y > topdownHeightMax) break;

        float hApprox = 0.0;
        if (sampleTopdownHeight(p.xz, hApprox)) {
            float distApprox = p.y - hApprox;
            if (distApprox > u_heightmapTransitionThreshold) {
                float minAngle = 0.12 + (1.0 - q) * 0.2;
                float angleScale = max(rayShallowness, minAngle);
                float ss = clamp(u_stepSizeScale, 0.1, 5.0);
                float stepApprox = max(20.0 + (1.0 - q) * 50.0, (distApprox / angleScale) * 0.9);
                // If the conservative distance estimate is increasing compared to the previous
                // sample we can safely grow the step size proportionally to the previous step.
                if (prevDistApprox > -1e19 && distApprox > prevDistApprox) {
                    // growth factor: be aggressive but bounded
                    float growthFactor = 1.5;
                    stepApprox = max(stepApprox, prevStep * growthFactor);
                }
                stepApprox = clamp(stepApprox * ss, 2.0 * ss, baseMaxF * ss);
                // update previous trackers for next iteration
                prevDistApprox = distApprox;
                prevStep = stepApprox;
                tPrev = t;
                t += stepApprox;
                continue;
            } else {
                // inside transition region: small steps, reset monotonic growth base
                prevDistApprox = distApprox;
                prevStep = 0.0;
            }
        } else {
            // no topdown sample available; reset monotonic growth tracking
            prevDistApprox = -1e20;
            prevStep = 0.0;
        }

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

    gl_FragColor = encodeU24(normalized);
}