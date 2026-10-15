#include "private_include/ShaderToyResourceTasks.h"
#include "vk_core/context/DeviceContext.h"
#include "vk_core/queue/Queue.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/descriptor_set/info_structs.h"
#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "shader_core/ShaderCompiler.h"
#include "file_utils.h"
#include "enums/enum_cast.h"
#include <algorithm>
#include <mutex>
#include <tuple>
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>

namespace {

using namespace lcf;
using namespace lcf::shader_toy;

constexpr vk::ImageSubresourceRange k_color_range {vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};
using SubmissionTokenList = std::vector<vkc::SubmissionToken>;

void initialize_image(vkc::CommandBufferProxy & cmd, const vkc::Image & image, const vkc::Buffer & staging) noexcept;
void initialize_buffer_image(vkc::CommandBufferProxy & cmd, ShaderToyImageResourceInfo & image_resource_info) noexcept;
void append_submission_token(SubmissionTokenList & wait_tokens, const ShaderToyImageResourceInfo & image_resource_info) noexcept;
std::error_code submit_dependencies(vkc::Queue & queue, vk::ArrayProxy<const vkc::SubmissionToken> wait_tokens) noexcept;
asio::awaitable<void> wait_for_retry();
std::error_code create_render_targets(std::span<const ShaderToyImageResourceInfo> images,
    ShaderToyPassInfo::RenderTargetList & render_targets) noexcept;
std::error_code create_channel_descriptor_set(vk::Device device, vkc::dsp::DescriptorSetAllocator & descriptor_allocator,
    vk::DescriptorType descriptor_type, uint32_t set, const ShaderToyInputChannels & input_channels,
    vkc::ShaderProgramInfo & shader_program_info, vkc::dsp::DescriptorSetProxy & descriptor_set_proxy) noexcept;
std::error_code create_pipeline(vk::Device device, const vkc::ShaderProgramInfo & shader_program_info,
    const vkc::ImageDescription & image_description, vkc::GraphicsPipeline & graphics_pipeline) noexcept;

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

std::expected<img::Image, std::error_code> load_image(const std::filesystem::path & path) noexcept
{
    auto expected_image_info = img::read_image_info(path);
    if (not expected_image_info) { return std::unexpected(expected_image_info.error().code()); }
    img::Image pixels;
    if (auto ec = pixels.loadFromFile(*expected_image_info, img::ImageFormat::eRGBA8Uint)) { return std::unexpected(ec); }
    return pixels;
}

std::expected<ShaderToyImageResourceInfo, std::error_code> upload_image(
    const ShaderToyTaskContext & context, const img::Image & pixels) noexcept
{
    const auto & memory_allocator = context.m_device_context.getMemoryAllocator();
    auto & gfx_task_queue = context.m_gfx_task_queue;
    vkc::Buffer staging;
    vk::BufferCreateInfo staging_info;
    staging_info.setSize(pixels.getDataSpan().size_bytes()).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    vkc::MemoryAllocationInfo staging_allocation_info;
    staging_allocation_info.setAccess(vkc::MemoryAccess::eHostSequentialWrite);
    if (auto ec = staging.create(memory_allocator, staging_info, staging_allocation_info)) { return std::unexpected(ec); }
    if (auto ec = staging.copyFromMemory(pixels.getDataSpan())) { return std::unexpected(ec); }
    if (auto ec = staging.flush()) { return std::unexpected(ec); }

    vk::ImageCreateInfo image_create_info;
    image_create_info.setImageType(vk::ImageType::e2D).setFormat(vk::Format::eR8G8B8A8Unorm)
        .setExtent({pixels.getWidth(), pixels.getHeight(), 1u}).setMipLevels(1u).setArrayLayers(1u)
        .setSamples(vk::SampleCountFlagBits::e1).setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
    if (context.m_queue_family_indices.front() != context.m_queue_family_indices.back()) {
        image_create_info.setSharingMode(vk::SharingMode::eConcurrent).setQueueFamilyIndices(context.m_queue_family_indices);
    }
    ShaderToyImageResourceInfo image_resource_info;
    if (auto ec = image_resource_info.m_image.create(memory_allocator, image_create_info, {})) { return std::unexpected(ec); }
    auto expected_noise_view = image_resource_info.m_image.createView(k_color_range, vk::ImageViewType::e2D);
    if (not expected_noise_view) { return std::unexpected(expected_noise_view.error()); }
    image_resource_info.m_sampled_view = std::move(*expected_noise_view);

    vkc::CommandBufferAllocateInfo allocation_info;
    allocation_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_batch = gfx_task_queue.allocateCommandBufferBatch(allocation_info);
    if (not expected_batch) { return std::unexpected(expected_batch.error()); }
    auto expected_cmd = expected_batch->acquireProxy();
    if (not expected_cmd) { return std::unexpected(expected_cmd.error()); }
    expected_cmd->begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    initialize_image(*expected_cmd, image_resource_info.m_image, staging);
    expected_cmd->end();
    expected_batch->collect(std::move(*expected_cmd));
    auto expected_submit = gfx_task_queue.submit(std::move(*expected_batch));
    gfx_task_queue.collectGarbage();
    if (not expected_submit) { return std::unexpected(expected_submit.error()); }
    image_resource_info.m_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_resource_info.m_stage_flags = vk::PipelineStageFlagBits2::eFragmentShader;
    image_resource_info.m_access_flags = vk::AccessFlagBits2::eShaderSampledRead;
    image_resource_info.m_submission_token_opt = *expected_submit;
    return image_resource_info;
}

asio::awaitable<std::expected<ShaderToyImageResourceInfo, std::error_code>> prepare_sampled_image(
    const ShaderToyTaskContext & context, const ShaderToyImageResourceInfo & image_resource_info)
{
    if (not image_resource_info.m_submission_token_opt) { co_return image_resource_info; }
    const auto & token = *image_resource_info.m_submission_token_opt;
    auto device = context.m_device_context.getDevice();
    while (device.getSemaphoreCounterValue(token.semaphore) < token.value) { co_await wait_for_retry(); }
    if (auto ec = submit_dependencies(context.m_graphics_queue, token)) { co_return std::unexpected(ec); }
    co_return image_resource_info;
}

std::expected<ShaderSource, std::error_code> load_shader_source(const std::filesystem::path & path) noexcept
{
    auto expected_code = read_file_as_string(path);
    if (not expected_code) { return std::unexpected(expected_code.error()); }
    return ShaderSource {std::move(*expected_code), path.stem().string(), path.parent_path()};
}

std::expected<sc::spirv::UnitList, std::error_code> compile_shader_source(const ShaderSource & source) noexcept
{
    static std::mutex compiler_mutex;
    std::scoped_lock lock {compiler_mutex};
    sc::ShaderCompiler compiler;
    compiler.addIncludeDirectory(SHADER_ASSETS_DIR);
    compiler.addIncludeDirectory(source.m_directory);
    return compiler.compileSlangSourceToSpv(source.m_code, source.m_module_name);
}

vkc::ShaderProgramInfo create_shader_program_info(const sc::spirv::UnitList & units)
{
    vkc::ShaderProgramInfo shader_program_info;
    for (const auto & unit : units) {
        auto stage = enum_cast<vk::ShaderStageFlagBits>(unit.getStage());
        vkc::ShaderStageInfo stage_info;
        stage_info.setStage(stage).setCode(unit.getCode()).setEntryPoint(unit.getEntryPoint());
        if (stage == vk::ShaderStageFlagBits::eFragment) { stage_info.addPushConstantRange(0u, sizeof(ShaderToyParams)); }
        shader_program_info.addStageInfo(std::move(stage_info));
    }
    return shader_program_info;
}

std::error_code create_buffer_resources(const ShaderToyTaskContext & context,
    const vkc::ImageDescription & image_description, uint32_t count,
    ImageResourceInfoList & image_resource_infos) noexcept
{
    const auto & device_context = context.m_device_context;
    const auto & queue_family_indices = context.m_queue_family_indices;
    vk::ImageCreateInfo image_create_info;
    image_create_info.setImageType(image_description.getType()).setFormat(image_description.getFormat())
        .setExtent(image_description.getExtent()).setMipLevels(image_description.getMipLevelCount())
        .setArrayLayers(image_description.getArrayLayerCount()).setSamples(image_description.getSampleCount())
        .setTiling(vk::ImageTiling::eOptimal).setUsage(image_description.getUsageFlags());
    if (queue_family_indices.front() != queue_family_indices.back()) {
        image_create_info.setSharingMode(vk::SharingMode::eConcurrent).setQueueFamilyIndices(queue_family_indices);
    }
    image_resource_infos.resize(count);
    for (auto & image_resource_info : image_resource_infos) {
        if (auto ec = image_resource_info.m_image.create(device_context.getMemoryAllocator(), image_create_info, {})) { return ec; }
        auto expected_view = image_resource_info.m_image.createView(k_color_range, vk::ImageViewType::e2D);
        if (not expected_view) { return expected_view.error(); }
        image_resource_info.m_sampled_view = std::move(*expected_view);
    }
    return {};
}

std::error_code initialize_buffer_resources(vkc::Queue & queue, ImageResourceInfoList & image_resource_infos) noexcept
{
    vkc::CommandBufferAllocateInfo allocation_info;
    allocation_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_batch = queue.allocateCommandBufferBatch(allocation_info);
    if (not expected_batch) { return expected_batch.error(); }
    auto expected_cmd = expected_batch->acquireProxy();
    if (not expected_cmd) { return expected_cmd.error(); }
    expected_cmd->begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    for (auto & image_resource_info : image_resource_infos) { initialize_buffer_image(*expected_cmd, image_resource_info); }
    expected_cmd->end();
    expected_batch->collect(std::move(*expected_cmd));
    auto expected_submit = queue.submit(std::move(*expected_batch));
    queue.collectGarbage();
    if (not expected_submit) { return expected_submit.error(); }
    for (auto & image_resource_info : image_resource_infos) { image_resource_info.m_submission_token_opt = *expected_submit; }
    return {};
}

std::error_code prepare_pass(const ShaderToyTaskContext & context,
    std::span<const ShaderToyImageResourceInfo> output_images, ShaderToyPassInfo & pass_info) noexcept
{
    auto device = context.m_device_context.getDevice();
    auto & descriptor_allocator = context.m_descriptor_allocator;
    auto shader_program_info = pass_info.m_shader_program_info;
    if (auto ec = create_channel_descriptor_set(device, descriptor_allocator, vk::DescriptorType::eSampledImage,
        0u, pass_info.m_input_channels, shader_program_info, pass_info.m_image_descriptor_set_proxy)) { return ec; }
    if (auto ec = create_channel_descriptor_set(device, descriptor_allocator, vk::DescriptorType::eSampler,
        1u, pass_info.m_input_channels, shader_program_info, pass_info.m_sampler_descriptor_set_proxy)) { return ec; }
    for (uint32_t binding = 0u; binding < pass_info.m_input_channels.size(); ++binding) {
        const auto & channel_opt = pass_info.m_input_channels[binding];
        if (not channel_opt) { continue; }
        pass_info.m_sampler_descriptor_set_proxy.setSampler(binding, channel_opt->m_sampler);
    }
    if (auto ec = create_render_targets(output_images, pass_info.m_render_targets)) { return ec; }
    return create_pipeline(device, shader_program_info,
        output_images.front().m_image.getDescription(), pass_info.m_gfx_pipeline);
}

asio::awaitable<std::expected<InstanceInfoPointer, std::error_code>> prepare_instance_info(
    const ShaderToyTaskContext & context, const InstanceInfoPointer & shader_toy_instance_info)
{
    auto device = context.m_device_context.getDevice();
    for (;;) {
        auto ec = shader_toy_instance_info->validate(device);
        if (not ec) { break; }
        if (ec != std::errc::resource_unavailable_try_again) { co_return std::unexpected(ec); }
        co_await wait_for_retry();
    }
    SubmissionTokenList wait_tokens;
    for (const auto & image_resource_infos : shader_toy_instance_info->viewResourceInfos()) {
        for (const auto & image_resource_info : image_resource_infos) { append_submission_token(wait_tokens, image_resource_info); }
    }
    if (auto ec = submit_dependencies(context.m_graphics_queue, wait_tokens)) { co_return std::unexpected(ec); }
    co_return shader_toy_instance_info;
}

}

namespace {

asio::awaitable<void> wait_for_retry()
{
    asio::steady_timer retry_timer {co_await asio::this_coro::executor};
    retry_timer.expires_after(std::chrono::milliseconds {1});
    co_await retry_timer.async_wait(asio::use_awaitable);
}

std::error_code submit_dependencies(vkc::Queue & queue, vk::ArrayProxy<const vkc::SubmissionToken> wait_tokens) noexcept
{
    if (wait_tokens.empty()) { return {}; }
    vkc::CommandBufferAllocateInfo allocation_info;
    allocation_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_batch = queue.allocateCommandBufferBatch(allocation_info);
    if (not expected_batch) { return expected_batch.error(); }
    auto expected_cmd = expected_batch->acquireProxy();
    if (not expected_cmd) { return expected_cmd.error(); }
    for (auto token : wait_tokens) {
        token.setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
        expected_cmd->addWaitInfo(token);
    }
    expected_cmd->begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    expected_cmd->end();
    expected_batch->collect(std::move(*expected_cmd));
    auto expected_submit = queue.submit(std::move(*expected_batch));
    if (not expected_submit) { return expected_submit.error(); }
    return {};
}

void initialize_image(vkc::CommandBufferProxy & cmd, const vkc::Image & image, const vkc::Buffer & staging) noexcept
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(image.handle()).setSubresourceRange(k_color_range)
        .setOldLayout(vk::ImageLayout::eUndefined).setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer).setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    vk::DependencyInfo dependency;
    dependency.setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(dependency);
    vk::BufferImageCopy region;
    region.setImageSubresource({vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u})
        .setImageExtent(image.getDescription().getExtent());
    cmd.copyBufferToImage(staging.handle(), image.handle(), vk::ImageLayout::eTransferDstOptimal, region);
    cmd.pinLease(staging.lease());
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer).setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader).setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead);
    cmd.pipelineBarrier2(dependency);
    cmd.pinLease(image.lease());
}

void initialize_buffer_image(vkc::CommandBufferProxy & cmd, ShaderToyImageResourceInfo & image_resource_info) noexcept
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(image_resource_info.m_image.handle()).setSubresourceRange(k_color_range)
        .setOldLayout(vk::ImageLayout::eUndefined).setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setDstStageMask(vk::PipelineStageFlagBits2::eClear).setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    cmd.pipelineBarrier2(vk::DependencyInfo {}.setImageMemoryBarriers(barrier));
    cmd.clearColorImage(image_resource_info.m_image.handle(), vk::ImageLayout::eTransferDstOptimal,
        vk::ClearColorValue {std::array<float, 4> {0, 0, 0, 0}}, k_color_range);
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eClear).setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader).setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead);
    cmd.pipelineBarrier2(vk::DependencyInfo {}.setImageMemoryBarriers(barrier));
    cmd.pinLease(image_resource_info.m_image.lease());
    image_resource_info.m_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_resource_info.m_stage_flags = vk::PipelineStageFlagBits2::eFragmentShader;
    image_resource_info.m_access_flags = vk::AccessFlagBits2::eShaderSampledRead;
}

void append_submission_token(SubmissionTokenList & wait_tokens, const ShaderToyImageResourceInfo & image_resource_info) noexcept
{
    if (not image_resource_info.m_submission_token_opt) { return; }
    const auto & token = *image_resource_info.m_submission_token_opt;
    auto position = std::ranges::find(wait_tokens, token.semaphore, &vkc::SubmissionToken::semaphore);
    if (position != wait_tokens.end()) {
        position->setValue(std::max(position->value, token.value));
        return;
    }
    wait_tokens.push_back(token);
    wait_tokens.back().setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
}

std::error_code create_render_targets(std::span<const ShaderToyImageResourceInfo> images, ShaderToyPassInfo::RenderTargetList & render_targets) noexcept
{
    const auto & description = images.front().m_image.getDescription();
    const auto & extent = description.getExtent();
    vkc::AttachmentSetInfoBuilder attachment_set_builder;
    auto color_key = attachment_set_builder.addColorAttachment();
    auto attachment_set = attachment_set_builder.build();
    vkc::RenderTargetInfo render_target_info {attachment_set};
    render_target_info.setExtent({extent.width, extent.height}).setFormat(color_key, description.getFormat());
    render_targets.resize(images.size());
    for (std::size_t index = 0u; index < images.size(); ++index) {
        auto & render_target = render_targets[index];
        if (auto ec = render_target.build(render_target_info)) { return ec; }
        if (auto ec = render_target.setColorAttachment(color_key, images[index].m_image)) { return ec; }
    }
    return {};
}

std::error_code create_channel_descriptor_set(vk::Device device, vkc::dsp::DescriptorSetAllocator & descriptor_allocator,
    vk::DescriptorType descriptor_type, uint32_t set, const ShaderToyInputChannels & input_channels,
    vkc::ShaderProgramInfo & shader_program_info, vkc::dsp::DescriptorSetProxy & descriptor_set_proxy) noexcept
{
    vkc::DescriptorSetLayoutInfo descriptor_set_layout_info;
    for (const auto & channel_opt : input_channels) {
        descriptor_set_layout_info.addBindingInfo(descriptor_type, channel_opt ? 1u : 0u, vk::ShaderStageFlagBits::eFragment);
    }
    vkc::dsp::DescriptorSetLayout descriptor_set_layout;
    if (auto ec = descriptor_set_layout.create(device, descriptor_set_layout_info)) { return ec; }
    if (auto ec = descriptor_set_proxy.create(descriptor_allocator, descriptor_set_layout)) { return ec; }
    descriptor_set_proxy.setSetIndex(set);
    shader_program_info.addDescriptorSetLayout(set, descriptor_set_layout.handle());
    return {};
}

std::error_code create_pipeline(vk::Device device, const vkc::ShaderProgramInfo & shader_program_info,
    const vkc::ImageDescription & image_description, vkc::GraphicsPipeline & graphics_pipeline) noexcept
{
    const auto & extent = image_description.getExtent();
    vkc::ViewportStateInfo viewport_info;
    viewport_info.addViewport(0, 0, extent.width, extent.height).addScissor(0, 0, extent.width, extent.height);
    vkc::ColorBlendStateInfo blend_info {1u};
    vkc::GraphicsPipelineInfo graphics_pipeline_info;
    graphics_pipeline_info.setShaderProgramInfo(shader_program_info)
        .setViewportStateInfo(viewport_info).setColorBlendStateInfo(blend_info);
    const std::array color_formats {image_description.getFormat()};
    vkc::DynamicRenderScopeInfo render_scope_info {color_formats, vk::Format::eUndefined, vk::Format::eUndefined, 0u};
    return graphics_pipeline.create(device, graphics_pipeline_info, render_scope_info);
}


}
