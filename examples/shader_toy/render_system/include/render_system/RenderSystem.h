#pragma once

#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/DeviceContext.h"  
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/pipeline/graphics/GraphicsPipeline.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/pipeline/graphics/StaticRender.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/memory/Image.h"
#include "vk_core/queue/Queue.h"
#include <vkc_config.h>
#include "render_system/WindowHandleHash.h"
#include <array>
#include <unordered_map>
#include <system_error>
#include <thread>

namespace lcf::shader_toy {

struct RenderSystemInfo
{
    vkc::wsi::WindowHandle m_window_handle;
};

class RenderSystem
{
    using Self = RenderSystem;
    using SwapchainMap = std::unordered_map<
        vkc::wsi::WindowHandle,
        vkc::wsi::probed::Swapchain,
        WindowHandleHash,
        WindowHandleEqual>;
public:
    ~RenderSystem() noexcept = default;
    RenderSystem() noexcept = default;
public:
    std::error_code create(const RenderSystemInfo &info) noexcept;
    std::error_code run() noexcept;
    void stop() noexcept;
    std::error_code resizeToFit(const vkc::wsi::WindowHandle &window_handle) noexcept;
private:
    vkc::InstanceContext m_instance_ctx;
    vkc::DeviceContext m_device_ctx;
    SwapchainMap m_swapchains;
    vkc::ColorAttachmentKey m_color_key;
    std::array<vkc::Image, 2> m_render_target_images;
    std::array<vkc::RenderTarget, 2> m_render_targets;
    vkc::StaticRender m_static_render;
    vkc::GraphicsPipeline m_graphics_pipeline;
    vkc::Queue m_graphics_queue;
    std::jthread m_worker;
};


}
