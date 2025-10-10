#pragma once

#include <utility>
#include "PointerDefs.h"

namespace lcf::render {
    class RenderTarget : public STDPointerDefs<RenderTarget>
    {
        using Self = RenderTarget;
    public:
        using Extent = std::pair<uint32_t, uint32_t>;
        RenderTarget() = default;
        virtual ~RenderTarget() = default;
        virtual void create() = 0;
        virtual bool isCreated() = 0;
        virtual bool isValid() const = 0;
        void requireUpdate() { m_need_to_update = true; }
        bool isUpdated() const { return not m_need_to_update; }
        Self & setMaximalExtent(uint32_t width, uint32_t height) { m_max_width = width; m_max_height = height; return *this; }
        uint32_t getMaximalWidth() const { return std::max(m_max_width, this->getWidth()); }
        uint32_t getMaximalHeight() const { return std::max(m_max_height, this->getHeight()); }
        Extent getMaximalExtent() const { return {this->getMaximalWidth(), this->getMaximalHeight()}; }
        Self & setExtent(uint32_t width, uint32_t height) { m_extent = {width, height}; return *this; }
        uint32_t getWidth() const { return m_extent.first; }
        uint32_t getHeight() const { return m_extent.second; }
        Extent getExtent() const { return m_extent; }
    protected:
        bool m_need_to_update = true;
        Extent m_extent;
        uint32_t m_max_width = 0;
        uint32_t m_max_height = 0;
    };
}