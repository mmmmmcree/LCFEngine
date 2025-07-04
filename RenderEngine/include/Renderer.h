#pragma once

#include "RHI/Context.h"
#include "RHI/RenderTarget.h"

namespace lcf {
    class Renderer
    {
    public:
        Renderer() = default;
        virtual ~Renderer() = default;
        virtual void setRenderTarget(render::RenderTarget * target) = 0;
        virtual void create() = 0;
        // virtual bool isCreated() const = 0;
        virtual bool isValid() const = 0;
        virtual void render() = 0;
    };
}