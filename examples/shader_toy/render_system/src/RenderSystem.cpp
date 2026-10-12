#include "render_system/RenderSystem.h"
#include "private_include/ShaderToyResourceTasks.h"
#include "private_include/RenderFrameContext.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/pipeline/graphics/entry.h"
#include "log.h"
#include <chrono>
#include <filesystem>
#include <utility>

namespace {

using namespace lcf;
using namespace lcf::shader_toy;

constexpr uint32_t k_width = 1440u;
constexpr uint32_t k_height = 788u;
using QueueFamilyIndices = ShaderToyTaskContext::QueueFamilyIndices;
using InstancePointer = std::unique_ptr<ShaderToyInstance>;
using FrameContextPointer = std::unique_ptr<RenderFrameContext>;
using ShaderPathList = std::array<std::filesystem::path, 2>;
using ShaderSourceList = std::array<ShaderSource, 2>;
using SpirvUnitLists = std::array<sc::spirv::UnitList, 2>;
using ShaderProgramInfoList = std::array<vkc::ShaderProgramInfo, 2>;

struct CloudSeaPacket
{
    InstanceInfoPointer m_instance_info = std::make_shared<ShaderToyInstanceInfo>();
    ShaderToyResourceId m_noise_resource_id;
    ShaderToyResourceId m_buffer_a_resource_id;
    ShaderToyResourceId m_output_resource_id;
};

using CloudSeaPacketPointer = std::shared_ptr<CloudSeaPacket>;

std::error_code render_frame(RenderFrameContext & context) noexcept;
AsyncTaskChain<std::monostate> create_cloud_sea_task_chain(const ShaderToyTaskContext & context,
    vkc::wsi::Swapchain & swapchain, InstancePointer & destination, FrameContextPointer & frame_context);
std::expected<CloudSeaPacketPointer, std::error_code> create_cloud_sea_packet(
    const ShaderToyTaskContext & context, const ShaderToyImageResourceInfo & noise_resource_info) noexcept;
std::expected<InstanceInfoPointer, std::error_code> prepare_cloud_sea_passes(
    const ShaderToyTaskContext & context, const CloudSeaPacketPointer & packet, const ShaderProgramInfoList & shader_program_infos) noexcept;

}

namespace lcf::shader_toy {

RenderFrameContext::RenderFrameContext(vkc::Queue & queue, vkc::wsi::Swapchain & swapchain, ShaderToyInstance & instance) noexcept :
    m_graphics_queue(queue), m_swapchain(swapchain), m_instance_p(&instance) {}

RenderSystem::RenderSystem() noexcept = default;

RenderSystem::~RenderSystem() noexcept
{
    this->stop();
}

std::error_code RenderSystem::create(const RenderSystemInfo & render_system_info) noexcept
{
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;
    device_ext_manifest
        .addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters>)
        .addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan13Features::synchronization2>);
    vkc::entry::register_context(inst_ext_manifest, device_ext_manifest);
    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks
        .setVerboseSink([](std::string_view message) { lcf_log_info(message); })
        .setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::entry::register_debug_utils(
        inst_ext_manifest,
        vkc::dbg::SeverityFlags::eError | vkc::dbg::SeverityFlags::eWarning | vkc::dbg::SeverityFlags::eVerbose,
        debug_callbacks);
    vkc::probe::CapabilityRegistry capabilities {inst_ext_manifest, device_ext_manifest};
    vkc::probe::register_capabilities(capabilities);
    vkc::entry::register_dynamic_render(device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info.setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    vkc::InstanceContextCreateInfo instance_info;
    instance_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);
    if (auto failure = m_instance_ctx.create(instance_info)) { return failure.code(); }

    auto expected_surface = vkc::wsi::create_surface(m_instance_ctx.getInstance(), render_system_info.m_window_handle);
    if (not expected_surface) { return expected_surface.error(); }
    auto & surface = *expected_surface;
    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    vkc::probe::configure_physical_device(physical_device_select_info);
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest);
    vkc::DeviceContextCreateInfo device_context_info;
    device_context_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPhysicalDeviceSelectInfo(physical_device_select_info);
    vkc::QueueRequest graphics_queue_request {
        vk::QueueFlagBits::eGraphics, {}, vkc::QueueSubmissionThreadTag {0}, 1.0f
    };
    vkc::QueueRequest present_queue_request {
        vk::QueueFlagBits::eGraphics, {}, vkc::QueueSubmissionThreadTag {1}, 1.0f, surface.get()
    };
    vkc::QueueRequest gfx_task_queue_request {
        vk::QueueFlagBits::eGraphics, {}, vkc::QueueSubmissionThreadTag {2}, 1.0f
    };
    auto graphics_queue_key = device_context_info.addQueueRequest(graphics_queue_request);
    auto present_queue_key = device_context_info.addQueueRequest(present_queue_request);
    auto gfx_task_queue_key = device_context_info.addQueueRequest(gfx_task_queue_request);
    if (auto ec = m_device_ctx.create(m_instance_ctx.getInstance(), device_context_info)) { return ec; }

    auto & swapchain = m_swapchains[render_system_info.m_window_handle];
    swapchain.setDesiredSurfaceFormat(vk::Format::eB8G8R8A8Unorm);
    if (auto ec = swapchain.create(
        std::move(surface), m_device_ctx.getPhysicalDevice(), m_device_ctx.getLogicalQueue(present_queue_key))) { return ec; }
    if (auto ec = m_graphics_queue.create(m_device_ctx.getLogicalQueue(graphics_queue_key))) { return ec; }
    if (auto ec = m_gfx_task_queue.create(m_device_ctx.getLogicalQueue(gfx_task_queue_key))) { return ec; }

    vkc::dsp::DescriptorSetAllocatorInfo allocator_info;
    allocator_info.setPoolSize({vk::DescriptorType::eSampledImage, 32u})
        .setPoolSize({vk::DescriptorType::eSampler, 32u})
        .setMaxSetsPerPool(16u);
    if (auto ec = m_descriptor_allocator.create(m_device_ctx.getDevice(), allocator_info)) { return ec; }
    m_task_context_up = std::make_unique<RenderTaskSystem>();
    const QueueFamilyIndices queue_family_indices {
        m_device_ctx.getLogicalQueue(gfx_task_queue_key).getFamilyIndex(),
        m_device_ctx.getLogicalQueue(graphics_queue_key).getFamilyIndex()
    };
    ShaderToyTaskContext task_context {
        *m_task_context_up, m_device_ctx, m_gfx_task_queue, m_graphics_queue,
        m_descriptor_allocator, queue_family_indices
    };
    m_task_context_up->enqueue(create_cloud_sea_task_chain(task_context, swapchain, m_shader_toy_instance_up, m_frame_context_up));
    return {};
}

std::error_code RenderSystem::run() noexcept
{
    if (m_worker.joinable()) { return std::make_error_code(std::errc::operation_in_progress); }
    if (not m_task_context_up) { return std::make_error_code(std::errc::operation_not_permitted); }
    auto & tasks = *m_task_context_up;
    if (auto ec = tasks.run()) { return ec; }
    m_worker = std::jthread([this](std::stop_token token) {
        auto & tasks = *m_task_context_up;
        std::stop_callback stop_callback(token, [&tasks] { tasks.stop(); });
        while (not token.stop_requested()) {
            if (auto ec = tasks.processTasks(static_cast<bool>(m_frame_context_up))) {
                lcf_log_error("render task failed: {}", ec.message());
                break;
            }
            if (token.stop_requested()) { break; }
            if (not m_frame_context_up) { continue; }
            if (auto ec = render_frame(*m_frame_context_up)) {
                if (ec == vkc::errc::surface_zero_size or ec == vkc::errc::present_skipped_for_resize) { continue; }
                lcf_log_error("render frame failed: {}", ec.message());
                break;
            }
        }
    });
    return {};
}

void RenderSystem::stop() noexcept
{
    m_worker.request_stop();
    if (m_task_context_up) { m_task_context_up->stop(); }
    if (m_worker.joinable()) { m_worker.join(); }
    if (m_task_context_up) { m_task_context_up->join(); }
    if (m_device_ctx.getDevice()) { m_device_ctx.getDevice().waitIdle(); }
}

std::error_code RenderSystem::resizeToFit(const vkc::wsi::WindowHandle & window_handle) noexcept
{
    auto it = m_swapchains.find(window_handle);
    if (it == m_swapchains.end()) { return {}; }
    return it->second.resizeToFit();
}

} // namespace lcf::shader_toy

namespace {

AsyncTaskChain<std::monostate> create_cloud_sea_task_chain(const ShaderToyTaskContext & context,
    vkc::wsi::Swapchain & swapchain, InstancePointer & destination, FrameContextPointer & frame_context)
{
    auto asset_directory = std::filesystem::path {SHADER_ASSETS_DIR} / "up-in-the-cloud-sea";
    auto gfx_executor = context.m_tasks.getGfxExecutor();
    auto cpu_executor = context.m_tasks.getCpuExecutor();
    auto packet_chain = AsyncTaskChain {asset_directory / "image.png", cpu_executor}
        .link(load_image)
        .link(gfx_executor, [context](const img::Image & pixels) { return upload_image(context, pixels); })
        .link([context](const ShaderToyImageResourceInfo & image_resource_info) { return create_cloud_sea_packet(context, image_resource_info); });
    auto packet_result = packet_chain.getResult();
    const ShaderPathList shader_paths {asset_directory / "buffer_a.slang", asset_directory / "image.slang"};
    return packet_chain
        .link(cpu_executor, [shader_paths](const CloudSeaPacketPointer &) -> std::expected<ShaderSourceList, std::error_code> {
            ShaderSourceList sources;
            for (std::size_t index = 0u; index < sources.size(); ++index) {
                auto expected_source = load_shader_source(shader_paths[index]);
                if (not expected_source) { return std::unexpected(expected_source.error()); }
                sources[index] = std::move(*expected_source);
            }
            return sources;
        })
        .link([](const ShaderSourceList & sources) -> std::expected<SpirvUnitLists, std::error_code> {
            SpirvUnitLists units;
            for (std::size_t index = 0u; index < sources.size(); ++index) {
                auto expected_units = compile_shader_source(sources[index]);
                if (not expected_units) { return std::unexpected(expected_units.error()); }
                units[index] = std::move(*expected_units);
            }
            return units;
        })
        .link([](const SpirvUnitLists & units) {
            ShaderProgramInfoList shader_program_infos;
            for (std::size_t index = 0u; index < units.size(); ++index) {
                shader_program_infos[index] = create_shader_program_info(units[index]);
            }
            return shader_program_infos;
        })
        .link(gfx_executor, [context, packet_result](const ShaderProgramInfoList & shader_program_infos) {
            return prepare_cloud_sea_passes(context, packet_result.getResult().value(), shader_program_infos);
        })
        .link(context.m_tasks.getRenderExecutor(), [context](const InstanceInfoPointer & shader_toy_instance_info) {
            return prepare_instance_info(context, shader_toy_instance_info);
        })
        .link([&destination](const InstanceInfoPointer & shader_toy_instance_info) -> std::error_code {
            auto instance_up = std::make_unique<ShaderToyInstance>();
            if (auto ec = instance_up->create(std::move(*shader_toy_instance_info))) { return ec; }
            destination = std::move(instance_up);
            return {};
        })
        .link([context, &swapchain, &destination, &frame_context](std::monostate) {
            frame_context = std::make_unique<RenderFrameContext>(context.m_graphics_queue, swapchain, *destination);
            lcf_log_info("up-in-the-cloud-sea ready: noise -> Buffer A (previous A) -> Image.");
        })
        .link(gfx_executor, [context](std::monostate) { context.m_gfx_task_queue.collectGarbage(); });
}

std::expected<CloudSeaPacketPointer, std::error_code> create_cloud_sea_packet(
    const ShaderToyTaskContext & context, const ShaderToyImageResourceInfo & noise_resource_info) noexcept
{
    auto packet = std::make_shared<CloudSeaPacket>();
    auto & shader_toy_instance_info = *packet->m_instance_info;
    packet->m_noise_resource_id = shader_toy_instance_info.addResource(noise_resource_info);
    vkc::ImageDescription buffer_a_description;
    buffer_a_description.setFormat(vk::Format::eR16G16B16A16Sfloat).setExtent({k_width, k_height, 1u})
        .addUsageFlags(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst);
    ImageResourceInfoList buffer_a_images;
    if (auto ec = create_buffer_resources(context, buffer_a_description, 2u, buffer_a_images)) { return std::unexpected(ec); }
    if (auto ec = initialize_buffer_resources(context.m_gfx_task_queue, buffer_a_images)) { return std::unexpected(ec); }
    packet->m_buffer_a_resource_id = shader_toy_instance_info.addResource(buffer_a_images);
    vkc::ImageDescription output_description;
    output_description.setFormat(vk::Format::eR8G8B8A8Unorm).setExtent({k_width, k_height, 1u})
        .addUsageFlags(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    ImageResourceInfoList output_images;
    if (auto ec = create_buffer_resources(context, output_description, 2u, output_images)) { return std::unexpected(ec); }
    packet->m_output_resource_id = shader_toy_instance_info.addResource(output_images);
    shader_toy_instance_info.setOutputResource(packet->m_output_resource_id);
    return packet;
}

std::expected<InstanceInfoPointer, std::error_code> prepare_cloud_sea_passes(
    const ShaderToyTaskContext & context, const CloudSeaPacketPointer & packet, const ShaderProgramInfoList & shader_program_infos) noexcept
{
    auto device = context.m_device_context.getDevice();
    vkc::Sampler noise_sampler;
    vkc::Sampler feedback_sampler;
    vkc::SamplerInfo noise_sampler_info;
    noise_sampler_info.setMinMagFilter(vk::Filter::eLinear, vk::Filter::eLinear).setAddressMode(vk::SamplerAddressMode::eRepeat);
    vkc::SamplerInfo feedback_sampler_info;
    feedback_sampler_info.setMinMagFilter(vk::Filter::eLinear, vk::Filter::eLinear).setAddressMode(vk::SamplerAddressMode::eClampToEdge);
    if (auto ec = noise_sampler.create(device, noise_sampler_info)) { return std::unexpected(ec); }
    if (auto ec = feedback_sampler.create(device, feedback_sampler_info)) { return std::unexpected(ec); }
    auto & shader_toy_instance_info = *packet->m_instance_info;
    ShaderToyPassInfo buffer_a_pass_info;
    buffer_a_pass_info.m_shader_program_info = shader_program_infos[0];
    buffer_a_pass_info.m_output_resource_id = packet->m_buffer_a_resource_id;
    buffer_a_pass_info.m_input_channels[0] = {packet->m_noise_resource_id.getFrameReference(ShaderToyInputFrame::eCurrent), noise_sampler};
    buffer_a_pass_info.m_input_channels[1] = {packet->m_buffer_a_resource_id.getFrameReference(ShaderToyInputFrame::ePrevious), feedback_sampler};
    if (auto ec = prepare_pass(context, shader_toy_instance_info.viewResourceInfos()[packet->m_buffer_a_resource_id.getIndex()], buffer_a_pass_info)) {
        return std::unexpected(ec);
    }
    shader_toy_instance_info.addPass(std::move(buffer_a_pass_info));
    ShaderToyPassInfo image_pass_info;
    image_pass_info.m_shader_program_info = shader_program_infos[1];
    image_pass_info.m_output_resource_id = packet->m_output_resource_id;
    image_pass_info.m_input_channels[0] = {packet->m_buffer_a_resource_id.getFrameReference(ShaderToyInputFrame::eCurrent), feedback_sampler};
    if (auto ec = prepare_pass(context, shader_toy_instance_info.viewResourceInfos()[packet->m_output_resource_id.getIndex()], image_pass_info)) {
        return std::unexpected(ec);
    }
    shader_toy_instance_info.addPass(std::move(image_pass_info));
    return packet->m_instance_info;
}

std::error_code render_frame(RenderFrameContext & context) noexcept
{
    auto & queue = context.m_graphics_queue;
    uint64_t frame = context.m_frame;
    auto & present_token = context.m_present_tokens[frame % context.m_present_tokens.size()];
    vkc::CommandBufferAllocateInfo allocation_info;
    allocation_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_batch = queue.allocateCommandBufferBatch(allocation_info);
    if (not expected_batch) { return expected_batch.error(); }
    auto expected_cmd = expected_batch->acquireProxy();
    if (not expected_cmd) { return expected_cmd.error(); }
    auto & cmd = *expected_cmd;
    ShaderToyParams params {
        .m_time = std::chrono::duration<float>(std::chrono::steady_clock::now() - context.m_start_time).count()
    };
    cmd.addWaitInfo(present_token);
    cmd.begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    if (auto ec = context.m_instance_p->build(cmd, params, frame)) { return ec; }
    cmd.end();
    expected_batch->collect(std::move(cmd));
    auto expected_submit = queue.submit(std::move(*expected_batch));
    queue.collectGarbage();
    if (not expected_submit) { return expected_submit.error(); }
    ++context.m_frame;

    const auto & image = context.m_instance_p->getOutputImage(frame);
    const auto & extent = image.getDescription().getExtent();
    std::array<vk::Offset3D, 2> offsets {
        vk::Offset3D {0, 0, 0},
        vk::Offset3D {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1}
    };
    auto expected_present = context.m_swapchain.present(offsets, image.handle(), image.lease(), *expected_submit);
    if (not expected_present) { return expected_present.error(); }
    present_token = *expected_present;
    return {};
}


}
