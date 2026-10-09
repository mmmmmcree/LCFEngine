#pragma once

#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/DeviceContext.h"  
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/descriptor_set/pool/DescriptorSetAllocator.h"
#include "vk_core/queue/Queue.h"
#include <vkc_config.h>
#include "render_system/WindowHandleHash.h"
#include <memory>
#include <unordered_map>
#include <system_error>
#include <thread>

namespace lcf::shader_toy {

class ShaderToyInstance;

struct RenderSystemInfo
{
    vkc::wsi::WindowHandle m_window_handle;
};

class RenderSystem
{
    using Self = RenderSystem;
    using InstancePointer = std::unique_ptr<ShaderToyInstance>;
    using SwapchainMap = std::unordered_map<
        vkc::wsi::WindowHandle,
        vkc::wsi::probed::Swapchain,
        WindowHandleHash,
        WindowHandleEqual>;
public:
    ~RenderSystem() noexcept;
    RenderSystem() noexcept;
public:
    std::error_code create(const RenderSystemInfo & render_system_info) noexcept;
    std::error_code run() noexcept;
    void stop() noexcept;
    std::error_code resizeToFit(const vkc::wsi::WindowHandle &window_handle) noexcept;
private:
    vkc::InstanceContext m_instance_ctx;
    vkc::DeviceContext m_device_ctx;
    SwapchainMap m_swapchains;
    vkc::Queue m_graphics_queue;
    vkc::dsp::DescriptorSetAllocator m_descriptor_allocator;
    InstancePointer m_shader_toy_instance_up;
    vkc::SubmissionToken m_upload_token;
    std::jthread m_worker;
};


}
