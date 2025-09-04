#pragma once

#include <utility>
#include "PointerDefs.h"

namespace lcf::render {
    class RenderTarget : public PointerDefs<RenderTarget>
    {
    public:
        RenderTarget() = default;
        virtual ~RenderTarget() = default;
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
        std::pair<uint32_t, uint32_t> getSize() const { return std::make_pair(getWidth(), getHeight()); }
        virtual void create() = 0;
        virtual bool isCreated() = 0;
        virtual void destroy() = 0;
        virtual bool isValid() const = 0;
        void requireUpdate() { m_need_to_update = true; }
        bool isUpdated() const { return not m_need_to_update; }
    protected:
        bool m_need_to_update = true;
    };
}