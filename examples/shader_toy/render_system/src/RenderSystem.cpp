#include "render_system/RenderSystem.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/pipeline/graphics/entry.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/pipeline/shader/info_structs.h"
#include "shader_core/ShaderCompiler.h"
#include "enums/enum_cast.h"
#include "log.h"
#include <chrono>
#include <filesystem>
#include <tuple>

namespace lcf {

constexpr uint32_t k_width = 1440;
constexpr uint32_t k_height = 788;

template <>
struct enum_mapping_traits<ShaderTypeFlagBits, vk::ShaderStageFlagBits>
{
    static constexpr std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits> mappings[] = {
        { ShaderTypeFlagBits::eVertex, vk::ShaderStageFlagBits::eVertex },
        { ShaderTypeFlagBits::eFragment, vk::ShaderStageFlagBits::eFragment },
    };
};

} // namespace lcf

namespace {

using namespace lcf;

struct ShaderToyParams
{
    std::array<float, 3> m_resolution {k_width, k_height, 1};
    float m_time {0.0f};
};

struct FrameContext
{
    vkc::Queue & graphics_queue;
    vkc::wsi::Swapchain & swapchain;
    vkc::DynamicRender & dynamic_render;
    vkc::GraphicsPipeline & graphics_pipeline;
    std::array<vkc::RenderTarget, 2> & render_targets;
    const vkc::ColorAttachmentKey & color_key;
    std::array<vkc::SubmissionToken, 2> & present_tokens;
    uint64_t frame = 0;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    float m_time = 0.0f;
};

std::error_code render_frame(FrameContext & context) noexcept
{
    auto & graphics_queue = context.graphics_queue;
    auto & swapchain = context.swapchain;
    auto & dynamic_render = context.dynamic_render;
    auto & graphics_pipeline = context.graphics_pipeline;
    auto & render_targets = context.render_targets;
    auto & color_key = context.color_key;
    auto & present_tokens = context.present_tokens;
    auto & frame = context.frame;
    auto & start_time = context.start_time;
    auto & m_time = context.m_time;
    m_time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start_time).count();
    vkc::CommandBufferAllocateInfo cmd_alloc_info;
    cmd_alloc_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_cmd_buffer_batch = graphics_queue.allocateCommandBufferBatch(cmd_alloc_info);
    if (not expected_cmd_buffer_batch) { return expected_cmd_buffer_batch.error(); }
    auto & cmd_buffer_batch = expected_cmd_buffer_batch.value();
    auto expected_cmd_proxy = cmd_buffer_batch.acquireProxy();
    if (not expected_cmd_proxy) { return expected_cmd_proxy.error(); }
    auto cmd = std::move(expected_cmd_proxy.value());
    auto & render_target = render_targets[frame % render_targets.size()];
    vk::PushConstantsInfo push_constants_info;
    ShaderToyParams params {
        .m_time = m_time
    };
    push_constants_info
        .setLayout(graphics_pipeline.getPipelineLayout())
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setOffset(0u)
        .setSize(sizeof(ShaderToyParams))
        .setValues<ShaderToyParams>(params);
    vk::CommandBufferBeginInfo cmd_begin_info {};
    cmd.begin(cmd_begin_info);
    dynamic_render.begin(cmd, render_target);
    graphics_pipeline.bind(cmd);
    cmd.pushConstants2(push_constants_info);
    cmd.draw(3u, 1u, 0u, 0u);
    dynamic_render.end(cmd);
    cmd.end();
    cmd.addWaitInfo(present_tokens[frame % present_tokens.size()]);
    cmd_buffer_batch.collect(std::move(cmd));

    auto expected_submit_result = graphics_queue.submit(std::move(cmd_buffer_batch));
    graphics_queue.collectGarbage();
    if (not expected_submit_result) { return expected_submit_result.error(); }

    const auto & attachment = render_target.getAttachment(color_key);
    const auto [width, height] = render_target.getMaxExtent();
    std::array<vk::Offset3D, 2> src_offsets {
        vk::Offset3D {0, 0, 0},
        vk::Offset3D {static_cast<int32_t>(width), static_cast<int32_t>(height), 1}
    };
    auto expected_present_result = swapchain.present(
        src_offsets,
        attachment.getImage().handle(),
        attachment.getImage().lease(),
        expected_submit_result.value());
    if (not expected_present_result) { return expected_present_result.error(); }
    present_tokens[frame % present_tokens.size()] = expected_present_result.value();
    ++frame;
    return {};
}

}


namespace lcf::shader_toy {

std::error_code RenderSystem::create(const RenderSystemInfo &info) noexcept
{
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;
    device_ext_manifest
        .addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters>);
    vkc::entry::register_context(inst_ext_manifest, device_ext_manifest);
    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks
        .setVerboseSink([](std::string_view message) { lcf_log_info(message); })
        .setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::entry::register_debug_utils(
        inst_ext_manifest,
        vkc::dbg::SeverityFlags::eError |
        vkc::dbg::SeverityFlags::eWarning |
        vkc::dbg::SeverityFlags::eVerbose,
        debug_callbacks);
    vkc::probe::CapabilityRegistry capabilities {inst_ext_manifest, device_ext_manifest};
    vkc::probe::register_capabilities(capabilities);

    vkc::entry::register_dynamic_render(device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);

    vkc::InstanceContextCreateInfo instance_info;
    instance_info
        .setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);

    if (auto failure = m_instance_ctx.create(instance_info)) { return failure.code(); }

    auto expected_surface = vkc::wsi::create_surface(m_instance_ctx.getInstance(), info.m_window_handle);
    if (not expected_surface) { return expected_surface.error(); }
    auto & surface = expected_surface.value();
    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    vkc::probe::configure_physical_device(physical_device_select_info);
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest);
    vkc::DeviceContextCreateInfo device_context_info;
    device_context_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPhysicalDeviceSelectInfo(physical_device_select_info);
    vkc::QueueRequest graphics_queue_request {
        vk::QueueFlagBits::eGraphics,
        {},
        vkc::QueueSubmissionThreadTag {0},
        1.0f
    };
    vkc::QueueRequest present_queue_request {
        vk::QueueFlagBits::eGraphics,
        {},
        vkc::QueueSubmissionThreadTag {1},
        1.0f,
        surface.get()
    };
    vkc::QueueKey graphics_queue_key = device_context_info.addQueueRequest(graphics_queue_request);
    vkc::QueueKey present_queue_key = device_context_info.addQueueRequest(present_queue_request);

    if (auto ec = m_device_ctx.create(m_instance_ctx.getInstance(), device_context_info)) { return ec; }

    auto & swapchain = m_swapchains[info.m_window_handle];
    swapchain.setDesiredSurfaceFormat(vk::Format::eB8G8R8A8Unorm);
    if (auto ec = swapchain.create(
        std::move(surface),
        m_device_ctx.getPhysicalDevice(),
        m_device_ctx.getLogicalQueue(present_queue_key))) { return ec; }
    //- end of render system setup

    sc::ShaderCompiler shader_compiler;
    shader_compiler.addIncludeDirectory(SHADER_ASSETS_DIR);
    auto expected_compile_result = shader_compiler.compileSlangSourceToSpv(
        std::filesystem::path {SHADER_ASSETS_DIR} / "test.slang");
    if (not expected_compile_result) { return expected_compile_result.error(); }
    lcf_log_info("Shader compiled successfully.");
    auto & spv_units = expected_compile_result.value();

    vkc::ShaderProgramInfo shader_program_info;
    for (const auto & spv_unit : spv_units) {
        vk::ShaderStageFlagBits stage = enum_cast<vk::ShaderStageFlagBits>(spv_unit.getStage());

        vkc::ShaderStageInfo shader_stage_info;
        shader_stage_info.setStage(stage)
            .setCode(spv_unit.getCode())
            .setEntryPoint(spv_unit.getEntryPoint());
        if (stage == vk::ShaderStageFlagBits::eFragment) {
            shader_stage_info.addPushConstantRange(0, sizeof(ShaderToyParams));
        }
        shader_program_info.addStageInfo(std::move(shader_stage_info));
    } 

    vkc::AttachmentSetInfoBuilder attachment_set_builder;
    m_color_key = attachment_set_builder.addColorAttachment();
    auto attachment_set = attachment_set_builder.build();
    vkc::RenderTargetInfo render_target_info {attachment_set};
    render_target_info
        .setExtent({k_width, k_height})
        .setFormat(m_color_key, vk::Format::eR8G8B8A8Unorm);
    for (auto & image : m_render_target_images) {
        vk::ImageCreateInfo image_info;
        image_info
            .setImageType(vk::ImageType::e2D)
            .setFormat(attachment_set.at(m_color_key).getFormat())
            .setExtent({k_width, k_height, 1u})
            .setMipLevels(1u)
            .setArrayLayers(1u)
            .setSamples(render_target_info.getSampleCount())
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
            .setInitialLayout(vk::ImageLayout::eUndefined);
        vkc::MemoryAllocationInfo allocation_info;
        allocation_info.setAccess(vkc::MemoryAccess::eDeviceLocal);
        if (auto ec = image.create(m_device_ctx.getMemoryAllocator(), image_info, allocation_info)) { return ec; }
    }
    for (auto & render_target : m_render_targets) {
        if (auto ec = render_target.build(render_target_info)) { return ec; }
    }
    for (std::size_t index = 0u; index < m_render_targets.size(); ++index) {
        if (auto ec = m_render_targets[index].setColorAttachment(m_color_key, m_render_target_images[index])) { return ec; }
    }

    vkc::ViewportStateInfo viewport_state_info;
    viewport_state_info.addViewport(0, 0, k_width, k_height).addScissor(0, 0, k_width, k_height);
    vkc::ColorBlendStateInfo color_blend_state_info {attachment_set.getColorAttachmentCount()};
    vkc::GraphicsPipelineInfo graphics_pipeline_info;
    graphics_pipeline_info
        .setShaderProgramInfo(shader_program_info)
        .setViewportStateInfo(viewport_state_info)
        .setColorBlendStateInfo(color_blend_state_info);

    vkc::DynamicRenderInfo dynamic_render_info {attachment_set};
    dynamic_render_info
        .setLoadStoreOp(m_color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitAttributes(m_color_key,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits2::eBlit,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageUsageFlagBits::eTransferSrc);
    if (auto ec = m_dynamic_render.create(dynamic_render_info)) { return ec; }
    if (auto ec = m_graphics_pipeline.create(m_device_ctx.getDevice(), graphics_pipeline_info, m_dynamic_render.makeScopeInfo())) { return ec; }
    if (auto ec = m_graphics_queue.create(m_device_ctx.getLogicalQueue(graphics_queue_key))) { return ec; }

    return {};
}

std::error_code RenderSystem::run() noexcept
{
    if (m_worker.joinable()) { return std::make_error_code(std::errc::operation_in_progress); }
    m_worker = std::jthread([this](std::stop_token token) {
        std::stop_callback stop_callback(token, [this] {
        });
        std::array<vkc::SubmissionToken, 2> present_tokens;
        FrameContext frame_context {
            m_graphics_queue,
            m_swapchains.begin()->second,
            m_dynamic_render,
            m_graphics_pipeline,
            m_render_targets,
            m_color_key,
            present_tokens
        };
        while (not token.stop_requested()) {
            auto ec = render_frame(frame_context);
            if (ec and ec != vkc::errc::surface_zero_size and ec != vkc::errc::present_skipped_for_resize) {
                lcf_log_error("render frame failed: {}", ec.message());
            }
        }
    });
    return {};
}

void RenderSystem::stop() noexcept
{
    m_worker.request_stop();
    if (m_worker.joinable()) {
        m_worker.join();
        m_device_ctx.getDevice().waitIdle();
    }
}

std::error_code RenderSystem::resizeToFit(const vkc::wsi::WindowHandle &window_handle) noexcept
{
    auto it = m_swapchains.find(window_handle);
    if (it == m_swapchains.end()) { return {}; }
    auto & swapchain = it->second;
    if (auto ec = swapchain.resizeToFit()) { return ec; }
    return {};
}

}
