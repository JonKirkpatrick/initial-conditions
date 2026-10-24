// Renderer.h
#pragma once
#include <memory>
#include "RenderFrame.h"

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void render(const RenderFrame& frame) = 0;
};