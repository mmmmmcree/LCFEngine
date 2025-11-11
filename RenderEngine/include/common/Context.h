#pragma once

#include "RenderTarget.h"
#include <vector>

namespace lcf::render {
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
    };
}