#pragma once

#include "vk_core/queue/Queue.h"
#include <vkc_config.h>
#include <array>
#include <chrono>

namespace lcf::shader_toy {

class ShaderToyInstance;

struct RenderFrameContext
{
    using Self = RenderFrameContext;
    using PresentTokenList = std::array<vkc::SubmissionToken, 2>;
public:
    ~RenderFrameContext() noexcept = default;
    RenderFrameContext(vkc::Queue & queue, vkc::wsi::Swapchain & swapchain, ShaderToyInstance & instance) noexcept;
    RenderFrameContext(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    RenderFrameContext(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = delete;
public:
    vkc::Queue & m_graphics_queue;
    vkc::wsi::Swapchain & m_swapchain;
    ShaderToyInstance * m_instance_p;
    PresentTokenList m_present_tokens;
    uint64_t m_frame = 0u;
    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

}
