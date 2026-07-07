#pragma once

#include "vk_core/memory/Image.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <utility>
#include <span>

namespace lcf::vkc {

class MemoryAllocator; 

class RenderTargetInfo;

class RenderTarget
{
    using ClearValueList = std::vector<vk::ClearValue>;
public:
    std::error_code create(const RenderTargetInfo & info) noexcept;
    void setRenderArea(const vk::Rect2D & render_area) noexcept;
    void setColorAttachmentClearValue(const vk::ClearColorValue & clear_value);
    void setDepthStencilAttachmentClearValue(const vk::ClearDepthStencilValue & clear_value);
    const vk::Rect2D & getRenderArea() const noexcept { return m_render_area; }
    const ClearValueList & getClearValues() const noexcept { return m_clear_values; }
    const uint32_t & getLayerCount() const noexcept { return m_layer_count; }
private:
    vk::Rect2D m_render_area;
    ClearValueList m_clear_values;
    uint32_t m_layer_count = 1;
};


} // namespace lcf::vkc
