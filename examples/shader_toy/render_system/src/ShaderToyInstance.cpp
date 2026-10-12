#include "private_include/ShaderToyInstance.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>

namespace {

using namespace lcf;
using namespace lcf::shader_toy;

constexpr auto k_no_producer = std::numeric_limits<std::size_t>::max();

struct ResourceUsage
{
    std::size_t m_producer = k_no_producer;
    bool m_self_dependent = false;
};

using ResourceUsageList = std::vector<ResourceUsage>;

uint64_t next_instance_id() noexcept;
void assert_resource_id(ShaderToyResourceId resource_id, uint64_t instance_id, std::size_t count) noexcept;
void assert_image_resource_info(const ShaderToyImageResourceInfo & image_resource_info) noexcept;
ResourceUsageList compile_resource_usage(const ShaderToyInstanceInfo & shader_toy_instance_info) noexcept;
std::error_code check_resource_readiness(vk::Device device, const ShaderToyInstanceInfo & shader_toy_instance_info) noexcept;
std::size_t input_image_index(std::size_t image_count, ShaderToyInputFrame input_frame, uint64_t frame) noexcept;
bool has_write_access(vk::AccessFlags2 access_flags) noexcept;
void transition_image(vkc::CommandBufferProxy & cmd, auto & image_resource,
    vk::ImageLayout layout, vk::PipelineStageFlags2 stage_flags, vk::AccessFlags2 access_flags) noexcept;
std::error_code prepare_descriptors(vkc::CommandBufferProxy & cmd, auto & pass, const auto & resources, uint64_t frame) noexcept;
void record_pass(vkc::CommandBufferProxy & cmd, auto & pass, auto & resources,
    const ShaderToyParams & params, uint64_t frame) noexcept;
void draw_render_target(vkc::CommandBufferProxy & cmd, const auto & pass,
    const vkc::RenderTarget & render_target, const ShaderToyParams & params) noexcept;

}

namespace lcf::shader_toy {

ShaderToyResourceId::ShaderToyResourceId(uint64_t instance_id, uint32_t index) noexcept :
    m_instance_id(instance_id), m_index(index)
{
}

ShaderToyResourceReference ShaderToyResourceId::getFrameReference(ShaderToyInputFrame frame) const noexcept
{
    return {*this, frame};
}

ShaderToyPassId::ShaderToyPassId(uint64_t instance_id, uint32_t index) noexcept :
    m_instance_id(instance_id), m_index(index)
{
}

ShaderToyInstanceInfo::ShaderToyInstanceInfo() noexcept : m_instance_id(next_instance_id())
{
}

ShaderToyInstanceInfo::ShaderToyInstanceInfo(Self && other) noexcept :
    m_instance_id(std::exchange(other.m_instance_id, next_instance_id())),
    m_resources(std::move(other.m_resources)),
    m_passes(std::move(other.m_passes)),
    m_output_resource_id(std::exchange(other.m_output_resource_id, {})),
    m_resource_types(std::move(other.m_resource_types)),
    m_is_valid(std::exchange(other.m_is_valid, false))
{
    other.m_resources.clear();
    other.m_passes.clear();
    other.m_resource_types.clear();
}

ShaderToyInstanceInfo & ShaderToyInstanceInfo::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_instance_id = std::exchange(other.m_instance_id, next_instance_id());
    m_resources = std::move(other.m_resources);
    m_passes = std::move(other.m_passes);
    m_output_resource_id = std::exchange(other.m_output_resource_id, {});
    m_resource_types = std::move(other.m_resource_types);
    m_is_valid = std::exchange(other.m_is_valid, false);
    other.m_resources.clear();
    other.m_passes.clear();
    other.m_resource_types.clear();
    return *this;
}

ShaderToyResourceId ShaderToyInstanceInfo::addResource(vk::ArrayProxy<const ShaderToyImageResourceInfo> image_resource_infos) noexcept
{
    assert(not image_resource_infos.empty());
    auto index = static_cast<uint32_t>(m_resources.size());
    m_resources.emplace_back(image_resource_infos.begin(), image_resource_infos.end());
    m_is_valid = false;
    return {m_instance_id, index};
}

ShaderToyPassId ShaderToyInstanceInfo::addPass(ShaderToyPassInfo pass_info) noexcept
{
    assert_resource_id(pass_info.m_output_resource_id, m_instance_id, m_resources.size());
    for (const auto & channel_opt : pass_info.m_input_channels) {
        if (not channel_opt) { continue; }
        assert_resource_id(channel_opt->m_source.m_resource_id, m_instance_id, m_resources.size());
    }
    auto index = static_cast<uint32_t>(m_passes.size());
    m_passes.emplace_back(std::move(pass_info));
    m_is_valid = false;
    return {m_instance_id, index};
}

ShaderToyInstanceInfo & ShaderToyInstanceInfo::setOutputResource(ShaderToyResourceId resource_id) noexcept
{
    assert_resource_id(resource_id, m_instance_id, m_resources.size());
    m_output_resource_id = resource_id;
    m_is_valid = false;
    return *this;
}

std::error_code ShaderToyInstanceInfo::validate(vk::Device device) const noexcept
{
    if (m_is_valid) { return {}; }
    auto resource_usage = compile_resource_usage(*this);
    if (auto ec = check_resource_readiness(device, *this)) { return ec; }
    m_resource_types.resize(m_resources.size());
    for (std::size_t index = 0u; index < m_resources.size(); ++index) {
        const auto & usage = resource_usage[index];
        auto resource_type = usage.m_producer == k_no_producer ? ShaderToyResourceType::eImage2D : ShaderToyResourceType::eBuffer;
        m_resource_types[index] = usage.m_self_dependent ? ShaderToyResourceType::eSelfDependentBuffer : resource_type;
    }
    m_is_valid = true;
    return {};
}

ShaderToyInstance::ShaderToyInstance(Self && other) noexcept :
    m_instance_id(std::exchange(other.m_instance_id, 0u)),
    m_resources(std::move(other.m_resources)),
    m_passes(std::move(other.m_passes)),
    m_output_resource_id(std::exchange(other.m_output_resource_id, {}))
{
}

ShaderToyInstance & ShaderToyInstance::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_passes = std::move(other.m_passes);
    m_resources = std::move(other.m_resources);
    m_instance_id = std::exchange(other.m_instance_id, 0u);
    m_output_resource_id = std::exchange(other.m_output_resource_id, {});
    return *this;
}

std::error_code ShaderToyInstance::create(ShaderToyInstanceInfo && shader_toy_instance_info) noexcept
{
    if (not shader_toy_instance_info.isValid()) { return std::make_error_code(std::errc::invalid_argument); }
    Self candidate;
    candidate.m_instance_id = shader_toy_instance_info.m_instance_id;
    candidate.m_output_resource_id = shader_toy_instance_info.m_output_resource_id;
    candidate.m_resources.resize(shader_toy_instance_info.m_resources.size());
    for (std::size_t index = 0u; index < candidate.m_resources.size(); ++index) {
        auto & resource = candidate.m_resources[index];
        resource.m_resource_type = shader_toy_instance_info.m_resource_types[index];
        for (auto & image_resource_info : shader_toy_instance_info.m_resources[index]) {
            resource.m_image_resources.push_back({std::move(image_resource_info.m_image), std::move(image_resource_info.m_sampled_view),
                image_resource_info.m_layout, image_resource_info.m_stage_flags, image_resource_info.m_access_flags});
        }
    }
    candidate.m_passes.resize(shader_toy_instance_info.m_passes.size());
    for (std::size_t index = 0u; index < candidate.m_passes.size(); ++index) {
        auto & pass = candidate.m_passes[index];
        auto & pass_info = shader_toy_instance_info.m_passes[index];
        for (uint32_t binding = 0u; binding < pass_info.m_input_channels.size(); ++binding) {
            const auto & channel_opt = pass_info.m_input_channels[binding];
            if (channel_opt) { pass.m_input_channels.push_back({binding, channel_opt->m_source}); }
        }
        pass.m_output_resource_id = pass_info.m_output_resource_id;
        pass.m_image_descriptor_set_proxy = std::move(pass_info.m_image_descriptor_set_proxy);
        pass.m_sampler_descriptor_set_proxy = std::move(pass_info.m_sampler_descriptor_set_proxy);
        pass.m_gfx_pipeline = std::move(pass_info.m_gfx_pipeline);
        pass.m_render_targets = std::move(pass_info.m_render_targets);
    }
    *this = std::move(candidate);
    shader_toy_instance_info = ShaderToyInstanceInfo {};
    return {};
}

std::error_code ShaderToyInstance::build(vkc::CommandBufferProxy & cmd, const ShaderToyParams & params, uint64_t frame) noexcept
{
    assert(m_instance_id != 0u);
    for (auto & pass : m_passes) {
        if (auto ec = prepare_descriptors(cmd, pass, m_resources, frame)) { return ec; }
    }
    for (auto & pass : m_passes) { record_pass(cmd, pass, m_resources, params, frame); }
    auto & images = m_resources[m_output_resource_id.getIndex()].m_image_resources;
    auto & image_resource = images[frame % images.size()];
    transition_image(cmd, image_resource, vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
    return {};
}

std::error_code ShaderToyInstance::setChannelSampler(ShaderToyPassId pass_id, uint32_t channel, vkc::Sampler sampler) noexcept
{
    assert(pass_id.getInstanceId() == m_instance_id and pass_id.getIndex() < m_passes.size());
    assert(sampler.handle());
    auto & pass = m_passes[pass_id.getIndex()];
    auto position = std::ranges::find(pass.m_input_channels, channel, &InputChannel::m_binding);
    assert(position != pass.m_input_channels.end());
    pass.m_sampler_descriptor_set_proxy.setSampler(channel, sampler);
    return {};
}

std::error_code ShaderToyInstance::setImageResource(ShaderToyResourceId resource_id,
    vk::ArrayProxy<const ShaderToyImageResourceInfo> image_resource_infos) noexcept
{
    assert_resource_id(resource_id, m_instance_id, m_resources.size());
    auto & resource = m_resources[resource_id.getIndex()];
    assert(resource.m_resource_type == ShaderToyResourceType::eImage2D and not image_resource_infos.empty());
    ImageResourceList image_resources;
    image_resources.reserve(image_resource_infos.size());
    for (const auto & image_resource_info : image_resource_infos) {
        image_resources.push_back({image_resource_info.m_image, image_resource_info.m_sampled_view,
            image_resource_info.m_layout, image_resource_info.m_stage_flags, image_resource_info.m_access_flags});
    }
    resource.m_image_resources = std::move(image_resources);
    return {};
}

const vkc::Image & ShaderToyInstance::getOutputImage(uint64_t frame) const noexcept
{
    const auto & images = m_resources[m_output_resource_id.getIndex()].m_image_resources;
    return images[frame % images.size()].m_image;
}

}

namespace {

uint64_t next_instance_id() noexcept
{
    static std::atomic_uint64_t instance_id {1u};
    return instance_id.fetch_add(1u, std::memory_order_relaxed);
}

void assert_resource_id(ShaderToyResourceId resource_id, uint64_t instance_id, std::size_t count) noexcept
{
    assert(resource_id.getInstanceId() == instance_id and resource_id.getIndex() < count);
}

void assert_image_resource_info(const ShaderToyImageResourceInfo & image_resource_info) noexcept
{
    assert(image_resource_info.m_image.handle() and image_resource_info.m_sampled_view.handle());
    assert(image_resource_info.m_sampled_view.makeInfo().image == image_resource_info.m_image.handle());
    const auto & description = image_resource_info.m_image.getDescription();
    assert(description.getUsageFlags() & vk::ImageUsageFlagBits::eSampled);
    assert(description.getSampleCount() == vk::SampleCountFlagBits::e1 and description.getType() == vk::ImageType::e2D);
    assert(description.getArrayLayerCount() == 1u and description.getMipLevelCount() == 1u);
    assert(image_resource_info.m_sampled_view.getViewType() == vk::ImageViewType::e2D);
    assert(not image_resource_info.m_submission_token_opt or image_resource_info.m_submission_token_opt->semaphore);
}

ResourceUsageList compile_resource_usage(const ShaderToyInstanceInfo & shader_toy_instance_info) noexcept
{
    auto resource_infos = shader_toy_instance_info.viewResourceInfos();
    auto pass_infos = shader_toy_instance_info.viewPassInfos();
    assert(not pass_infos.empty());
    assert_resource_id(shader_toy_instance_info.getOutputResourceId(), shader_toy_instance_info.getInstanceId(), resource_infos.size());
    const auto & output_images = resource_infos[shader_toy_instance_info.getOutputResourceId().getIndex()];
    assert(output_images.front().m_image.getDescription().getUsageFlags() & vk::ImageUsageFlagBits::eTransferSrc);
    for (const auto & image_resource_infos : resource_infos) {
        for (const auto & image_resource_info : image_resource_infos) {
            assert_image_resource_info(image_resource_info);
            assert(image_resource_info.m_image.getDescription() == image_resource_infos.front().m_image.getDescription());
        }
    }
    ResourceUsageList resource_usage(resource_infos.size());
    for (std::size_t index = 0u; index < pass_infos.size(); ++index) {
        const auto & pass_info = pass_infos[index];
        auto & usage = resource_usage[pass_info.m_output_resource_id.getIndex()];
        assert(usage.m_producer == k_no_producer);
        usage.m_producer = index;
        const auto & images = resource_infos[pass_info.m_output_resource_id.getIndex()];
        assert(images.front().m_image.getDescription().getUsageFlags() & vk::ImageUsageFlagBits::eColorAttachment);
        assert(pass_info.m_gfx_pipeline.handle() and pass_info.m_gfx_pipeline.getPipelineLayout());
        assert(pass_info.m_image_descriptor_set_proxy.getSetIndex() == 0u and pass_info.m_sampler_descriptor_set_proxy.getSetIndex() == 1u);
        assert(pass_info.m_render_targets.size() == images.size());
        for (std::size_t slot = 0u; slot < images.size(); ++slot) {
            auto attachments = pass_info.m_render_targets[slot].viewAttachments();
            assert(attachments.size() == 1u and attachments.front().getImage().handle() == images[slot].m_image.handle());
        }
    }
    assert(resource_usage[shader_toy_instance_info.getOutputResourceId().getIndex()].m_producer != k_no_producer);
    for (std::size_t index = 0u; index < pass_infos.size(); ++index) {
        for (const auto & channel_opt : pass_infos[index].m_input_channels) {
            if (not channel_opt) { continue; }
            assert(channel_opt->m_sampler.handle());
            auto resource_index = channel_opt->m_source.m_resource_id.getIndex();
            auto & usage = resource_usage[resource_index];
            if (channel_opt->m_source.m_frame == ShaderToyInputFrame::eCurrent) {
                assert(usage.m_producer == k_no_producer or usage.m_producer < index);
                continue;
            }
            const auto & images = resource_infos[resource_index];
            assert(usage.m_producer != k_no_producer);
            assert(usage.m_producer > index or images.size() >= 2u);
            for (const auto & image_resource_info : images) { assert(image_resource_info.m_layout != vk::ImageLayout::eUndefined); }
            usage.m_self_dependent |= usage.m_producer == index;
        }
    }
    for (std::size_t index = 0u; index < resource_infos.size(); ++index) {
        if (resource_usage[index].m_producer != k_no_producer) { continue; }
        for (const auto & image_resource_info : resource_infos[index]) { assert(image_resource_info.m_layout != vk::ImageLayout::eUndefined); }
    }
    return resource_usage;
}

std::error_code check_resource_readiness(vk::Device device, const ShaderToyInstanceInfo & shader_toy_instance_info) noexcept
{
    try {
        for (const auto & image_resource_infos : shader_toy_instance_info.viewResourceInfos()) {
            for (const auto & image_resource_info : image_resource_infos) {
                if (not image_resource_info.m_submission_token_opt) { continue; }
                const auto & token = *image_resource_info.m_submission_token_opt;
                if (device.getSemaphoreCounterValue(token.semaphore) < token.value) {
                    return std::make_error_code(std::errc::resource_unavailable_try_again);
                }
            }
        }
    } catch (const vk::SystemError & error) { return error.code(); }
    return {};
}

std::size_t input_image_index(std::size_t image_count, ShaderToyInputFrame input_frame, uint64_t frame) noexcept
{
    auto current = static_cast<std::size_t>(frame % image_count);
    return input_frame == ShaderToyInputFrame::eCurrent ? current : (current + image_count - 1u) % image_count;
}

bool has_write_access(vk::AccessFlags2 access_flags) noexcept
{
    constexpr auto k_write_access = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderStorageWrite |
        vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
        vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite;
    return bool(access_flags & k_write_access);
}

void transition_image(vkc::CommandBufferProxy & cmd, auto & image_resource,
    vk::ImageLayout layout, vk::PipelineStageFlags2 stage_flags, vk::AccessFlags2 access_flags) noexcept
{
    cmd.pinLease(image_resource.m_image.lease());
    if (image_resource.m_layout == layout and not has_write_access(image_resource.m_access_flags) and not has_write_access(access_flags)) {
        image_resource.m_stage_flags |= stage_flags;
        image_resource.m_access_flags |= access_flags;
        return;
    }
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(image_resource.m_image.handle()).setSubresourceRange(image_resource.m_sampled_view.getRange())
        .setOldLayout(image_resource.m_layout).setNewLayout(layout)
        .setSrcStageMask(image_resource.m_stage_flags).setSrcAccessMask(image_resource.m_access_flags)
        .setDstStageMask(stage_flags).setDstAccessMask(access_flags)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    cmd.pipelineBarrier2(vk::DependencyInfo {}.setImageMemoryBarriers(barrier));
    image_resource.m_layout = layout;
    image_resource.m_stage_flags = stage_flags;
    image_resource.m_access_flags = access_flags;
}

std::error_code prepare_descriptors(vkc::CommandBufferProxy & cmd, auto & pass, const auto & resources, uint64_t frame) noexcept
{
    for (const auto & channel : pass.m_input_channels) {
        const auto & images = resources[channel.m_source.m_resource_id.getIndex()].m_image_resources;
        const auto & image_resource = images[input_image_index(images.size(), channel.m_source.m_frame, frame)];
        pass.m_image_descriptor_set_proxy.setImage(channel.m_binding, image_resource.m_sampled_view, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    if (auto ec = pass.m_image_descriptor_set_proxy.updateIfDirty(cmd)) { return ec; }
    return pass.m_sampler_descriptor_set_proxy.updateIfDirty(cmd);
}

void record_pass(vkc::CommandBufferProxy & cmd, auto & pass, auto & resources,
    const ShaderToyParams & params, uint64_t frame) noexcept
{
    for (const auto & channel : pass.m_input_channels) {
        auto & images = resources[channel.m_source.m_resource_id.getIndex()].m_image_resources;
        auto & image_resource = images[input_image_index(images.size(), channel.m_source.m_frame, frame)];
        transition_image(cmd, image_resource, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead);
    }
    auto & images = resources[pass.m_output_resource_id.getIndex()].m_image_resources;
    auto slot = static_cast<std::size_t>(frame % images.size());
    transition_image(cmd, images[slot], vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite);
    pass.m_gfx_pipeline.bind(cmd);
    auto pipeline_layout = pass.m_gfx_pipeline.getPipelineLayout();
    pass.m_image_descriptor_set_proxy.bind(cmd, vk::PipelineBindPoint::eGraphics, pipeline_layout);
    pass.m_sampler_descriptor_set_proxy.bind(cmd, vk::PipelineBindPoint::eGraphics, pipeline_layout);
    draw_render_target(cmd, pass, pass.m_render_targets[slot], params);
}

void draw_render_target(vkc::CommandBufferProxy & cmd, const auto & pass,
    const vkc::RenderTarget & render_target, const ShaderToyParams & params) noexcept
{
    const auto & extent = render_target.getMaxExtent();
    const ShaderToyParams::Resolution resolution {static_cast<float>(extent.width), static_cast<float>(extent.height), 1.0f};
    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.setImageView(render_target.viewAttachments().front().getImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare).setStoreOp(vk::AttachmentStoreOp::eStore);
    vk::RenderingInfo rendering_info;
    rendering_info
        .setRenderArea(render_target.getRenderArea())
        .setLayerCount(render_target.getLayerCount())
        .setColorAttachments(color_attachment);
    cmd.beginRendering(rendering_info);
    auto pipeline_layout = pass.m_gfx_pipeline.getPipelineLayout();
    cmd.pushConstants<ShaderToyParams>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0u, params);
    cmd.pushConstants<float>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, offsetof(ShaderToyParams, m_resolution), resolution);
    cmd.draw(3u, 1u, 0u, 0u);
    cmd.endRendering();
}

}
