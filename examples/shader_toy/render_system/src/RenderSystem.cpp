#include "render_system/RenderSystem.h"
#include "private_include/ShaderToyInstance.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/pipeline/graphics/entry.h"
#include "vk_core/descriptor_set/info_structs.h"
#include "vk_core/sampler/Sampler.h"
#include "shader_core/ShaderCompiler.h"
#include "image/Image.h"
#include "enums/enum_cast.h"
#include "log.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <span>
#include <tuple>

namespace {

using namespace lcf;
using namespace lcf::shader_toy;

constexpr uint32_t k_width = 1440u;
constexpr uint32_t k_height = 788u;
constexpr vk::ImageSubresourceRange k_color_range {vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};

struct CloudSeaResources
{
    ShaderToyInstanceInfo m_shader_toy_instance_info;
    vkc::SubmissionToken m_upload_token;
};

struct CloudSeaPassResources
{
    using DescriptorSetProxyList = std::array<vkc::dsp::DescriptorSetProxy, 2>;

    vkc::ShaderProgramInfo m_shader_program_info;
    DescriptorSetProxyList m_ds_proxies;
};

struct FrameContext
{
    using PresentTokenList = std::array<vkc::SubmissionToken, 2>;

    vkc::Queue & m_graphics_queue;
    vkc::wsi::Swapchain & m_swapchain;
    ShaderToyInstance & m_instance;
    vkc::SubmissionToken m_upload_token;
    PresentTokenList m_present_tokens;
    uint64_t m_frame = 0u;
    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

std::error_code render_frame(FrameContext & context) noexcept;
std::expected<CloudSeaResources, std::error_code> create_cloud_sea_resources(
    const vkc::DeviceContext & device_context, vkc::Queue & graphics_queue,
    vkc::dsp::DescriptorSetAllocator & descriptor_allocator) noexcept;
void initialize_image(vkc::CommandBufferProxy & cmd, const vkc::Image & image, const vkc::Buffer * staging_p) noexcept;

}

namespace lcf {

template <>
struct enum_mapping_traits<ShaderTypeFlagBits, vk::ShaderStageFlagBits>
{
    static constexpr std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits> mappings[] = {
        { ShaderTypeFlagBits::eVertex, vk::ShaderStageFlagBits::eVertex },
        { ShaderTypeFlagBits::eFragment, vk::ShaderStageFlagBits::eFragment },
    };
};

}

namespace lcf::shader_toy {

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
    auto graphics_queue_key = device_context_info.addQueueRequest(graphics_queue_request);
    auto present_queue_key = device_context_info.addQueueRequest(present_queue_request);
    if (auto ec = m_device_ctx.create(m_instance_ctx.getInstance(), device_context_info)) { return ec; }

    auto & swapchain = m_swapchains[render_system_info.m_window_handle];
    swapchain.setDesiredSurfaceFormat(vk::Format::eB8G8R8A8Unorm);
    if (auto ec = swapchain.create(
        std::move(surface), m_device_ctx.getPhysicalDevice(), m_device_ctx.getLogicalQueue(present_queue_key))) { return ec; }
    if (auto ec = m_graphics_queue.create(m_device_ctx.getLogicalQueue(graphics_queue_key))) { return ec; }

    vkc::dsp::DescriptorSetAllocatorInfo allocator_info;
    allocator_info.setPoolSize({vk::DescriptorType::eSampledImage, 32u})
        .setPoolSize({vk::DescriptorType::eSampler, 32u})
        .setMaxSetsPerPool(16u);
    if (auto ec = m_descriptor_allocator.create(m_device_ctx.getDevice(), allocator_info)) { return ec; }
    auto expected_resources = create_cloud_sea_resources(m_device_ctx, m_graphics_queue, m_descriptor_allocator);
    if (not expected_resources) { return expected_resources.error(); }
    m_upload_token = expected_resources->m_upload_token;
    m_shader_toy_instance_up = std::make_unique<ShaderToyInstance>();
    if (auto ec = m_shader_toy_instance_up->create(m_device_ctx.getDevice(), std::move(expected_resources->m_shader_toy_instance_info))) { return ec; }
    lcf_log_info("up-in-the-cloud-sea ready: noise -> Buffer A (previous A) -> Image.");
    return {};
}

std::error_code RenderSystem::run() noexcept
{
    if (not m_shader_toy_instance_up) { return std::make_error_code(std::errc::operation_not_permitted); }
    if (m_worker.joinable()) { return std::make_error_code(std::errc::operation_in_progress); }
    m_worker = std::jthread([this](std::stop_token token) {
        FrameContext context {
            .m_graphics_queue = m_graphics_queue,
            .m_swapchain = m_swapchains.begin()->second,
            .m_instance = *m_shader_toy_instance_up,
            .m_upload_token = m_upload_token
        };
        while (not token.stop_requested()) {
            if (auto ec = render_frame(context)) {
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
    if (m_worker.joinable()) { m_worker.join(); }
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

std::error_code render_frame(FrameContext & context) noexcept
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
    cmd.addWaitInfo(context.m_upload_token).addWaitInfo(present_token);
    cmd.begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    if (auto ec = context.m_instance.build(cmd, params, frame)) { return ec; }
    cmd.end();
    expected_batch->collect(std::move(cmd));
    auto expected_submit = queue.submit(std::move(*expected_batch));
    queue.collectGarbage();
    if (not expected_submit) { return expected_submit.error(); }
    ++context.m_frame;

    const auto & image = context.m_instance.getOutputImage(frame);
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

std::expected<CloudSeaResources, std::error_code> create_cloud_sea_resources(
    const vkc::DeviceContext & device_context, vkc::Queue & graphics_queue,
    vkc::dsp::DescriptorSetAllocator & descriptor_allocator) noexcept
{
    CloudSeaResources cloud_sea_resources;
    auto & shader_toy_instance_info = cloud_sea_resources.m_shader_toy_instance_info;
    const auto & memory_allocator = device_context.getMemoryAllocator();
    auto device = device_context.getDevice();
    std::filesystem::path asset_directory = std::filesystem::path {SHADER_ASSETS_DIR} / "up-in-the-cloud-sea";
    img::Image pixels;
    auto expected_image_info = img::read_image_info(asset_directory / "image.png");
    if (not expected_image_info) { return std::unexpected(expected_image_info.error().code()); }
    if (auto ec = pixels.loadFromFile(*expected_image_info, img::ImageFormat::eRGBA8Uint)) { return std::unexpected(ec); }

    vkc::Buffer staging;
    vk::BufferCreateInfo staging_info;
    staging_info.setSize(pixels.getDataSpan().size_bytes()).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    vkc::MemoryAllocationInfo staging_allocation_info;
    staging_allocation_info.setAccess(vkc::MemoryAccess::eHostSequentialWrite);
    if (auto ec = staging.create(memory_allocator, staging_info, staging_allocation_info)) { return std::unexpected(ec); }
    if (auto ec = staging.copyFromMemory(pixels.getDataSpan())) { return std::unexpected(ec); }
    if (auto ec = staging.flush()) { return std::unexpected(ec); }

    std::vector<TextureKey> texture_keys;
    for (uint32_t index = 0u; index < 3u; ++index) {
        auto extent = index == 0u ? vk::Extent2D {pixels.getWidth(), pixels.getHeight()} : vk::Extent2D {k_width, k_height};
        auto format = index == 1u ? vk::Format::eR16G16B16A16Sfloat : vk::Format::eR8G8B8A8Unorm;
        auto usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
        if (index != 0u) { usage |= vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc; }
        vk::ImageCreateInfo image_create_info;
        image_create_info.setImageType(vk::ImageType::e2D).setFormat(format)
            .setExtent({extent.width, extent.height, 1u}).setMipLevels(1u).setArrayLayers(1u)
            .setSamples(vk::SampleCountFlagBits::e1).setTiling(vk::ImageTiling::eOptimal)
            .setUsage(usage).setInitialLayout(vk::ImageLayout::eUndefined);
        vkc::MemoryAllocationInfo allocation_info;
        allocation_info.setAccess(vkc::MemoryAccess::eDeviceLocal);
        ShaderToyTextureInfo::ImagePair images;
        auto image_count = index == 0u ? 1u : 2u;
        for (uint32_t image_index = 0u; image_index < image_count; ++image_index) {
            if (auto ec = images[image_index].create(memory_allocator, image_create_info, allocation_info)) { return std::unexpected(ec); }
        }
        if (index == 0u) {
            texture_keys.emplace_back(shader_toy_instance_info.addTextureInfo(ShaderToyTextureInfo {std::move(images[0])}));
        } else {
            texture_keys.emplace_back(shader_toy_instance_info.addTextureInfo(ShaderToyTextureInfo {std::move(images)}));
        }
    }
    auto noise_texture_key = texture_keys[0];
    auto buffer_a_texture_key = texture_keys[1];
    auto image_texture_key = texture_keys[2];

    std::array<vkc::Sampler, 2> samplers;
    vkc::SamplerInfo noise_sampler_info;
    noise_sampler_info.setMinMagFilter(vk::Filter::eLinear, vk::Filter::eLinear).setAddressMode(vk::SamplerAddressMode::eRepeat);
    vkc::SamplerInfo feedback_sampler_info;
    feedback_sampler_info.setMinMagFilter(vk::Filter::eLinear, vk::Filter::eLinear).setAddressMode(vk::SamplerAddressMode::eClampToEdge);
    if (auto ec = samplers[0].create(device, noise_sampler_info)) { return std::unexpected(ec); }
    if (auto ec = samplers[1].create(device, feedback_sampler_info)) { return std::unexpected(ec); }

    sc::ShaderCompiler shader_compiler;
    shader_compiler.addIncludeDirectory(SHADER_ASSETS_DIR);
    const std::array shader_paths {asset_directory / "buffer_a.slang", asset_directory / "image.slang"};
    std::array<CloudSeaPassResources, 2> pass_resources;
    for (std::size_t pass_index = 0u; pass_index < pass_resources.size(); ++pass_index) {
        auto & shader_program_info = pass_resources[pass_index].m_shader_program_info;
        auto & descriptor_set_proxies = pass_resources[pass_index].m_ds_proxies;
        auto pass_samplers = std::span {samplers}.subspan(pass_index);
        auto expected_compile = shader_compiler.compileSlangSourceToSpv(shader_paths[pass_index]);
        if (not expected_compile) { return std::unexpected(expected_compile.error()); }
        for (const auto & unit : *expected_compile) {
            auto stage = enum_cast<vk::ShaderStageFlagBits>(unit.getStage());
            vkc::ShaderStageInfo stage_info;
            stage_info.setStage(stage).setCode(unit.getCode()).setEntryPoint(unit.getEntryPoint());
            if (stage == vk::ShaderStageFlagBits::eFragment) { stage_info.addPushConstantRange(0u, sizeof(ShaderToyParams)); }
            shader_program_info.addStageInfo(std::move(stage_info));
        }
        for (uint32_t set = 0u; set < descriptor_set_proxies.size(); ++set) {
            vkc::DescriptorSetLayoutInfo layout_info;
            for (std::size_t channel = 0u; channel < pass_samplers.size(); ++channel) {
                layout_info.addBindingInfo(set == 0u ? vk::DescriptorType::eSampledImage : vk::DescriptorType::eSampler,
                    1u, vk::ShaderStageFlagBits::eFragment);
            }
            vkc::dsp::DescriptorSetLayout layout;
            if (auto ec = layout.create(device, layout_info)) { return std::unexpected(ec); }
            auto & descriptor_set_proxy = descriptor_set_proxies[set];
            if (auto ec = descriptor_set_proxy.create(descriptor_allocator, layout)) { return std::unexpected(ec); }
            descriptor_set_proxy.setSetIndex(set);
            shader_program_info.addDescriptorSetLayout(set, layout.handle());
        }
        for (uint32_t channel = 0u; channel < pass_samplers.size(); ++channel) {
            descriptor_set_proxies[1].setSampler(channel, pass_samplers[channel]);
        }
    }

    ShaderToyPassInfo buffer_a_pass_info {buffer_a_texture_key, std::move(pass_resources[0].m_shader_program_info)};
    buffer_a_pass_info.setChannelInfo<0>({noise_texture_key})
        .setChannelInfo<1>({buffer_a_texture_key, TextureFrame::ePrevious});
    for (auto & descriptor_set_proxy : pass_resources[0].m_ds_proxies) { buffer_a_pass_info.setDescriptorSetProxy(std::move(descriptor_set_proxy)); }
    shader_toy_instance_info.addPassInfo(std::move(buffer_a_pass_info));

    ShaderToyPassInfo image_pass_info {image_texture_key, std::move(pass_resources[1].m_shader_program_info)};
    image_pass_info.setChannelInfo<0>({buffer_a_texture_key});
    for (auto & descriptor_set_proxy : pass_resources[1].m_ds_proxies) { image_pass_info.setDescriptorSetProxy(std::move(descriptor_set_proxy)); }
    shader_toy_instance_info.addPassInfo(std::move(image_pass_info));

    vkc::CommandBufferAllocateInfo allocation_info;
    allocation_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_batch = graphics_queue.allocateCommandBufferBatch(allocation_info);
    if (not expected_batch) { return std::unexpected(expected_batch.error()); }
    auto expected_cmd = expected_batch->acquireProxy();
    if (not expected_cmd) { return std::unexpected(expected_cmd.error()); }
    auto & cmd = *expected_cmd;
    cmd.begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    const auto texture_infos = shader_toy_instance_info.viewTextureInfos();
    for (std::size_t index = 0u; index < texture_infos.size(); ++index) {
        for (const auto & image : texture_infos[index].viewImages()) {
            initialize_image(cmd, image, index == 0u ? &staging : nullptr);
        }
    }
    cmd.end();
    expected_batch->collect(std::move(cmd));
    auto expected_submit = graphics_queue.submit(std::move(*expected_batch));
    if (not expected_submit) { return std::unexpected(expected_submit.error()); }
    cloud_sea_resources.m_upload_token = *expected_submit;
    return cloud_sea_resources;
}

void initialize_image(vkc::CommandBufferProxy & cmd, const vkc::Image & image, const vkc::Buffer * staging_p) noexcept
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(image.handle()).setSubresourceRange(k_color_range)
        .setOldLayout(vk::ImageLayout::eUndefined).setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer).setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    vk::DependencyInfo dependency;
    dependency.setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(dependency);
    if (staging_p) {
        vk::BufferImageCopy region;
        region.setImageSubresource({vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u})
            .setImageExtent(image.getDescription().getExtent());
        cmd.copyBufferToImage(staging_p->handle(), image.handle(), vk::ImageLayout::eTransferDstOptimal, region);
        cmd.pinLease(staging_p->lease());
    } else {
        cmd.clearColorImage(image.handle(), vk::ImageLayout::eTransferDstOptimal,
            vk::ClearColorValue {std::array<float, 4> {0.0f, 0.0f, 0.0f, 0.0f}}, k_color_range);
    }
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer).setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader).setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead);
    cmd.pipelineBarrier2(dependency);
    cmd.pinLease(image.lease());
}

}
