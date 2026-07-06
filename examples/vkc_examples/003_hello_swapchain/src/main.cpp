#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/entry.h"
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/WSI/Swapchain.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/Image.h"
#include "vk_core/error.h"
#include "win/Window.h"
#include <atomic>
#include <thread>
#include <variant>
#include <array>
#include <optional>
#include "log.h"

using namespace lcf;
namespace stdv = std::views;

//- lcf::win::WindowHandle and vkc::wsi::WindowHandle are same-shaped variants;
//- the window library is Vulkan-agnostic, so the mapping happens here at the
//- call site (one std::visit), keeping the two libraries fully decoupled.
vkc::wsi::WindowHandle to_wsi_window_handle(const win::WindowHandle & window_handle) noexcept
{
    return std::visit([](const auto & handle) -> vkc::wsi::WindowHandle {
        using T = std::decay_t<decltype(handle)>;
        if constexpr (std::is_same_v<T, win::win32::WindowHandle>) {
            return vkc::wsi::win32::WindowHandle(handle.m_hinstance, handle.m_hwnd);
        } else if constexpr (std::is_same_v<T, win::xcb::WindowHandle>) {
            return vkc::wsi::xcb::WindowHandle(handle.m_connection, handle.m_window);
        } else if constexpr (std::is_same_v<T, win::wayland::WindowHandle>) {
            return vkc::wsi::wayland::WindowHandle(handle.m_display, handle.m_surface);
        } else {
            return vkc::wsi::metal::WindowHandle(handle.m_layer);
        }
    }, window_handle);
}

std::optional<vkc::Image> create_single_color_image(vkc::RenderDeviceContext & device_context, const std::array<float, 4> & color) noexcept;

int main()
{
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::entry::register_context(inst_ext_manifest, device_ext_manifest);
    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks.setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::entry::register_debug_utils(inst_ext_manifest, vkc::dbg::SeverityFlags::eError | vkc::dbg::SeverityFlags::eWarning | vkc::dbg::SeverityFlags::eVerbose, debug_callbacks);
    vkc::entry::register_surface(inst_ext_manifest);
    vkc::entry::register_swapchain(device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);

    vkc::InstanceContextCreateInfo instance_info;
    instance_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);

    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPreferredType(vk::PhysicalDeviceType::eDiscreteGpu);
    vkc::DeviceContextCreateInfo device_context_info;
    device_context_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPhysicalDeviceSelectInfo(physical_device_select_info);

    vkc::InstanceContext instance_context;
    if (auto ec = instance_context.create(instance_info)) {
        lcf_log_error("Failed to create instance_context: {}", ec.message());
        return 1;
    }
    lcf_log_info("InstanceContext created successfully.");

    vkc::RenderDeviceContext render_device_context;
    if (auto ec = render_device_context.create(instance_context.getInstance(), device_context_info)) {
        lcf_log_error("Failed to create render_device_context: {}", ec.message());
        return 1;
    }

    win::WindowCreateInfo window_info;
    window_info.setTitle("hello swapchain");
    win::Window window;
    if (auto ec = window.create(window_info)) {
        lcf_log_error("Failed to create window: {}", ec.message());
        return 1;
    }

    if (auto ec = window.show()) {
        lcf_log_error("Failed to show window: {}", ec.message());
        return 1;
    }

    vkc::wsi::WindowHandle wsi_window_handle = to_wsi_window_handle(window.handle());
    vkc::wsi::Swapchain swapchain;
    if (auto ec = swapchain.create(
        instance_context.getInstance(),
        render_device_context.getPhysicalDevice(),
        render_device_context.getDevice(),
        render_device_context.getGraphicsQueueContext().getFamilyIndex(),
        wsi_window_handle))
    {
        lcf_log_error("Failed to create swapchain: {}", ec.message());
        return 1;
    }
    lcf_log_info("Swapchain created successfully.");

    auto white_image_opt = create_single_color_image(render_device_context, {1.0f, 1.0f, 1.0f, 1.0f});
    auto red_image_opt = create_single_color_image(render_device_context, {1.0f, 1.0f, 0.0f, 1.0f});
    if (not white_image_opt or not red_image_opt) {
        lcf_log_error("Failed to create image source image.");
        return 1;
    }
    auto & white_image = *white_image_opt;
    auto & red_image = *red_image_opt;
    std::array<vk::Image, 2> images = {white_image, red_image};
    const std::array<vk::Offset3D, 2> src_offsets {{ {0, 0, 0}, {1, 1, 1} }};

    std::atomic<bool> running {true};
    std::thread render_thread([&] {
        static uint64_t frame = 0;
        while (running.load(std::memory_order_relaxed)) {
            auto expected_present_result = swapchain.present(src_offsets, images[frame++ % 2]);
            if (expected_present_result) { continue; }
            auto ec = expected_present_result.error();
            if (ec == vkc::errc::surface_zero_size or ec == vkc::errc::present_skipped_for_resize) { continue; }
            lcf_log_error("present failed: {}", ec.message());
        }
    });

    /*
        - resize during a Win32 modal drag reaches us synchronously through this
        - callback (the pollEvents loop is blocked then). resizeToFit replays the
        - cached frame at the new size, serialized against present() internally.
        - if don't register this callback, the swapchain will resize correctly, but the window edge might appear black during the resizing
    */
    window.setResizeCallback([&swapchain](const win::ResizeEvent &) {
        if (auto ec = swapchain.resizeToFit()) { lcf_log_error("resizeToFit failed: {}", ec.message()); }
    });

    while (running.load(std::memory_order_relaxed)) {
        for (const win::WindowEvent & event : window.pollEvents()) {
            if (std::holds_alternative<win::CloseEvent>(event)) {
                running.store(false, std::memory_order_relaxed);
            }
        }
    }

    render_thread.join();
    render_device_context.getDevice().waitIdle();
    return 0;
}

std::optional<vkc::Image> create_single_color_image(vkc::RenderDeviceContext & device_context, const std::array<float, 4> & color) noexcept
{
    vk::Device device = device_context.getDevice();
    vk::PhysicalDevice physical_device = device_context.getPhysicalDevice();
    uint32_t gfx_family = device_context.getGraphicsQueueContext().getFamilyIndex();
    vk::Queue gfx_queue = device_context.getGraphicsQueueContext().getQueue();

    vkc::Image image;
    try {
        vk::ImageCreateInfo image_info;
        image_info.setImageType(vk::ImageType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setExtent({1u, 1u, 1u})
            .setMipLevels(1u)
            .setArrayLayers(1u)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc)
            .setInitialLayout(vk::ImageLayout::eUndefined);
        vkc::MemoryAllocationInfo mem_alloc_info;
        mem_alloc_info.setAccess(vkc::MemoryAccess::eDeviceLocal);
        auto expected_image = device_context.getMemoryContext().createImage(image_info, mem_alloc_info);
        if (not expected_image) { return std::nullopt; }
        image = std::move(*expected_image);

        // one-time submit: undefined -> transferDst, clear white, transferDst -> transferSrc
        vk::UniqueCommandPool pool = device.createCommandPoolUnique({vk::CommandPoolCreateFlagBits::eTransient, gfx_family});
        vk::CommandBuffer cmd = device.allocateCommandBuffers({pool.get(), vk::CommandBufferLevel::ePrimary, 1u}).front();

        vk::ImageSubresourceRange range {vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};
        vk::ImageMemoryBarrier to_dst, to_src;
        to_dst.setImage(image)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSubresourceRange(range)
            .setSrcAccessMask(vk::AccessFlagBits::eNone)
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        to_src.setImage(image)
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSubresourceRange(range)
            .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits::eTransferRead);

        vk::ClearColorValue clear_color_value {color};
        cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, to_dst);
        cmd.clearColorImage(image, vk::ImageLayout::eTransferDstOptimal, clear_color_value, range);
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, to_src);
        cmd.end();

        vk::UniqueFence fence = device.createFenceUnique({});
        gfx_queue.submit(vk::SubmitInfo().setCommandBuffers(cmd), fence.get());
        auto wait_result = device.waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
        if (wait_result != vk::Result::eSuccess) {
            lcf_log_error("waitForFences failed while preparing white image.");
            return std::nullopt;
        }
    } catch (const vk::SystemError & e) {
        lcf_log_error("create_white_image failed: {}", e.what());
        return std::nullopt;
    }
    return image;
}