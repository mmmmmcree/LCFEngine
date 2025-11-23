#pragma once

#include <utility>
#include "PointerDefs.h"
#include <atomic>
#include <algorithm>

namespace lcf::render {
    class RenderTarget : public STDPointerDefs<RenderTarget>
    {
        using Self = RenderTarget;
    public:
        using Extent = std::pair<uint32_t, uint32_t>;
        RenderTarget() = default;
        virtual ~RenderTarget() = default;
        void setActive() noexcept { m_silent = false; }
        void setSelient() noexcept { m_silent = true; }
        Self & setMaximalExtent(uint32_t width, uint32_t height) { m_max_width = width; m_max_height = height; return *this; }
        uint32_t getMaximalWidth() const { return std::max(m_max_width, this->getWidth()); }
        uint32_t getMaximalHeight() const { return std::max(m_max_height, this->getHeight()); }
        Extent getMaximalExtent() const { return {this->getMaximalWidth(), this->getMaximalHeight()}; }
        Self & setExtent(uint32_t width, uint32_t height) { m_extent = {width, height}; return *this; }
        uint32_t getWidth() const { return m_extent.first; }
        uint32_t getHeight() const { return m_extent.second; }
        Extent getExtent() const { return m_extent; }
    protected:
        Extent m_extent;
        uint32_t m_max_width = 0;
        uint32_t m_max_height = 0;
        std::atomic_bool m_silent = false;
    };
}