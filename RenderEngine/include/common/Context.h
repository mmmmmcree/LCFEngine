#pragma once

#include "RenderTarget.h"
#include <vector>

namespace lcf {
    class RenderWindow;
}

namespace lcf::render {
    class Context
    {
    public:
        virtual ~Context() = default;
        virtual void create() = 0;
        virtual bool isCreated() const = 0;
        virtual bool isValid() const = 0;
        virtual void registerWindow(RenderWindow * window) = 0;
    };
}