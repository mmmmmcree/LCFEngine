#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/entry.h"
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/WSI/Swapchain.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/Image.h"
#include "vk_core/error.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/pipeline/graphics/StaticGraphicsPipeline.h"
#include "vk_core/pipeline/graphics/StaticRendering.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/command/CommandBufferAllocateInfo.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "win/Window.h"
#include "shader_core/config.h"
#include "shader_core/ShaderCompiler.h"
#include <atomic>
#include <thread>
#include <variant>
#include <array>
#include <optional>
#include "log.h"
#include "enums/enum_cast.h"

using namespace lcf;
namespace stdv = std::views;

vkc::wsi::WindowHandle to_wsi_window_handle(const win::WindowHandle & window_handle) noexcept;

namespace lcf {
template <>
struct enum_mapping_traits<ShaderTypeFlagBits, vk::ShaderStageFlagBits>
{
    static constexpr std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits> mappings[] = {
        { ShaderTypeFlagBits::eVertex, vk::ShaderStageFlagBits::eVertex },
        { ShaderTypeFlagBits::eFragment, vk::ShaderStageFlagBits::eFragment },
    };
};
} // namespace lcf

int main()
{
    std::filesystem::path shader_assets_dir = SHADER_ASSETS_DIR;
    lcf::sc::Config::instance()
        .registerVirtualPath("shaders", shader_assets_dir)
        .setDefaultGlslEntryPoint("main");
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
    //- in this example, we use shader constants to draw a triangle, so we should enable shaderDrawParameters feature
    device_ext_manifest.addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters>);

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

    vkc::RenderDeviceContext render_device_context;
    if (auto ec = render_device_context.create(instance_context.getInstance(), device_context_info)) {
        lcf_log_error("Failed to create render_device_context: {}", ec.message());
        return 1;
    }

    win::WindowCreateInfo window_info;
    window_info.setTitle("hello static pipeline");
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
    vk::Queue present_queue = render_device_context.getDevice().getQueue(render_device_context.getGraphicsQueueContext().getFamilyIndex(), 1);
    if (auto ec = swapchain.create(
        instance_context.getInstance(),
        render_device_context.getPhysicalDevice(),
        render_device_context.getDevice(),
        render_device_context.getGraphicsQueueContext().getFamilyIndex(),
        // render_device_context.getGraphicsQueueContext().getQueue(),
        present_queue,
        wsi_window_handle))
    {
        lcf_log_error("Failed to create swapchain: {}", ec.message());
        return 1;
    }

    //- compile shader and create shader_program_info
    sc::ShaderCompiler shader_compiler;
    auto expected_compile_result = shader_compiler.compileSlangSourceToSpv("shaders://triangle.slang");
    if (not expected_compile_result) {
        lcf_log_error("Failed to compile shader: {}", expected_compile_result.error().message());
        return 1;
    }
    lcf_log_info("Shader compiled successfully.");
    auto & spv_units = expected_compile_result.value();

    vkc::ShaderProgramInfo shader_program_info;
    for (const auto & spv_unit : spv_units) {
        vkc::ShaderStageInfo shader_stage_info;
        shader_stage_info.setStage(enum_cast<vk::ShaderStageFlagBits>(spv_unit.getStage()))
            .setCode(spv_unit.getCode())
            .setEntryPoint(spv_unit.getEntryPoint());
        shader_program_info.addStageInfo(std::move(shader_stage_info));
    }

    //- create render targets
    vkc::RenderTargetInfo render_target_info;
    auto [width, height] = window.getPixelSize();
    render_target_info.setExtent({width, height})
        .addColorAttachment(vk::Format::eR8G8B8A8Unorm);
    std::array<vkc::Image, 2> render_target_images;
    std::array<vkc::RenderTarget, 2> render_targets;
    for (auto & image : render_target_images) {
        vk::ImageCreateInfo image_info;
        image_info.setImageType(vk::ImageType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setExtent({width, height, 1u})
            .setMipLevels(1u)
            .setArrayLayers(1u)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
            .setInitialLayout(vk::ImageLayout::eUndefined);
        vkc::MemoryAllocationInfo mem_alloc_info;
        mem_alloc_info.setAccess(vkc::MemoryAccess::eDeviceLocal);
        if (auto ec = image.create(render_device_context.getMemoryAllocator(), image_info, mem_alloc_info)) {
            lcf_log_error("Failed to create image: {}", ec.message());
            return 1;
        }
    }
    for (auto & render_target : render_targets) {
        if (auto ec = render_target.build(render_target_info)) {
            lcf_log_error("Failed to create render_target: {}", ec.message());
            return 1;
        }
    }
    for (auto && [render_target, image] : stdv::zip(render_targets, render_target_images)) {
        if (auto ec = render_target.setColorAttachment(0, image)) {
            lcf_log_error("Failed to set color attachment: {}", ec.message());
            return 1;
        }
    }
    
    vk::Device device = render_device_context.getDevice();
    //- create static rendering
    vkc::SubpassDescriptionInfo subpass_info;
    subpass_info.setBindPoint(vk::PipelineBindPoint::eGraphics)
        .addColorAttachment(vkc::AttachmentReferenceInfo{0, vk::ImageLayout::eColorAttachmentOptimal});
    vkc::AttachmentStateInfo attachment_state_info;
    attachment_state_info.setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setLayouts(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
    vkc::RenderingInfo rendering_info;
    rendering_info.addSubpass(subpass_info)
        .addAttachmentState(attachment_state_info);
    vkc::StaticRendering static_rendering;
    if (auto ec = static_rendering.create(device, rendering_info, render_target_info)) {
        lcf_log_error("Failed to create static_rendering: {}", ec.message());
        return 1;
    }
    //- create static graphics pipeline with static rendering

    vkc::ViewportStateInfo viewport_state_info;
    viewport_state_info.addViewport(0, 0, width, height)
        .addScissor(0, 0, width, height);
    vkc::ColorBlendStateInfo color_blend_state_info;
    color_blend_state_info.addAttachment();   // 1 个 color attachment,匹配 subpass 的 colorAttachmentCount
    vkc::GraphicsPipelineInfo graphic_pipeline_info;
    vkc::StaticGraphicsPipeline static_graphics_pipeline;
    graphic_pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
        .setViewportStateInfo(viewport_state_info)
        .setColorBlendStateInfo(color_blend_state_info);
    if (auto ec = static_graphics_pipeline.create(device, graphic_pipeline_info, static_rendering)) {
        lcf_log_error("Failed to create static_graphics_pipeline: {}", ec.message());
        return 1;
    }

    //- render loop
    auto & gfx_queue_context = render_device_context.getGraphicsQueueContext();
    std::atomic<bool> running {true};
    std::thread render_thread([&] {
        static uint64_t frame = 0;
        vk::SemaphoreSubmitInfo present_blit_finish_semaphore_info;
        while (running.load(std::memory_order_relaxed)) {
            vkc::CommandBufferAllocateInfo cmd_alloc_info;
            cmd_alloc_info.setLevel(vk::CommandBufferLevel::ePrimary)
                .setCount(1);
            auto expected_cmd_buffer_batch = gfx_queue_context.allocateCommandBufferBatch(cmd_alloc_info);
            if (not expected_cmd_buffer_batch) {
                lcf_log_error("Failed to allocate command buffer: {}", expected_cmd_buffer_batch.error().message());
                continue;
            }
            vkc::CommandBufferBatch & cmd_buffer_batch = expected_cmd_buffer_batch.value();
            auto expected_cmd_buffer_proxy = cmd_buffer_batch.acquireProxy();
            if (not expected_cmd_buffer_proxy) {
                lcf_log_error("Failed to acquire proxy: {}", expected_cmd_buffer_proxy.error().message());
                continue;
            }
            vkc::CommandBufferProxy & cmd = expected_cmd_buffer_proxy.value();
            auto & render_target = render_targets[frame % 2];
            vk::CommandBufferBeginInfo cmd_begin_info {};
            cmd.begin(cmd_begin_info);
            static_rendering.begin(cmd, render_target);
            static_graphics_pipeline.bind(cmd);
            cmd.draw(3, 1, 0, 0);
            static_rendering.end(cmd);
            cmd.end();
            cmd.addWaitInfo(present_blit_finish_semaphore_info);
            cmd_buffer_batch.collect(std::move(cmd));
            auto expected_submit_result = gfx_queue_context.submit(std::move(cmd_buffer_batch));
            if (not expected_submit_result) { continue; }
            auto & submit_semaphore_info = expected_submit_result.value();
            std::array<vk::Offset3D, 2> src_offsets {{ {0, 0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height), 1} }};
            const vkc::Image & present_image = render_target.getAttachment(0).getImage();
            auto expected_present_result = swapchain.present(src_offsets, present_image, present_image.lease(), submit_semaphore_info);
            if (expected_present_result) {
                present_blit_finish_semaphore_info = expected_present_result.value();
                continue;
            }
            auto ec = expected_present_result.error();
            if (ec == vkc::errc::surface_zero_size or ec == vkc::errc::present_skipped_for_resize) { continue; }
            lcf_log_error("present failed: {}", ec.message());
        }
    });

    window.setResizeCallback([&swapchain](const win::ResizeEvent &) {
        if (auto ec = swapchain.resizeToFit(); ec and ec != vkc::errc::surface_zero_size) {
            lcf_log_error("resizeToFit failed: {}", ec.message());
        }
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