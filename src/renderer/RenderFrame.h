// RenderFrame.h
#pragma once
#include "renderer/gl/RenderTypes.h"

struct RenderFrame {
    CameraBlock                 camera;
    EnvironmentBlock            environment;
    AtmosphereBlock             atmosphere;
    ShadowBlock                 shadows;
    LightingBlock               lighting;

    std::span<const OrbData>    orbs;
    TerrainFrame                terrain;
    OceanFrame                  ocean;

    DebugFlags                  debug;
}