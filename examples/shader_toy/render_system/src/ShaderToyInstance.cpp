#include "private_include/ShaderToyInstance.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <utility>

namespace {

using namespace lcf;
using namespace lcf::shader_toy;

constexpr vk::ImageSubresourceRange k_color_range {vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};

uint64_t next_texture_set_id() noexcept;
std::error_code validate_instance_info(const ShaderToyInstanceInfo & shader_toy_instance_info) noexcept;

void transition_image(
    vkc::CommandBufferProxy & cmd,
    auto & image,
    vk::ImageLayout layout,
    vk::PipelineStageFlags2 stage_flags,
    vk::AccessFlags2 access_flags) noexcept;

std::error_code build_pass(
    vkc::CommandBufferProxy & cmd,
    ShaderToyPass & pass,
    const ShaderToyParams & params,
    uint64_t frame,
    auto & textures) noexcept;

}

namespace lcf::shader_toy {

ShaderToyTextureInfo::ShaderToyTextureInfo(vkc::Image image) noexcept
{
    m_images.emplace_back(std::move(image));
}

ShaderToyTextureInfo::ShaderToyTextureInfo(ImagePair images) noexcept
{
    for (auto & image : images) { m_images.emplace_back(std::move(image)); }
}

ShaderToyTextureInfo & ShaderToyTextureInfo::setInitialState(
    vk::ImageLayout layout, vk::PipelineStageFlags2 stage_flags, vk::AccessFlags2 access_flags) noexcept
{
    m_layout = layout;
    m_stage_flags = stage_flags;
    m_access_flags = access_flags;
    return *this;
}

ShaderToyPassInfo::ShaderToyPassInfo(TextureKey output_texture_key, vkc::ShaderProgramInfo shader_program_info) noexcept :
    m_shader_program_info(std::move(shader_program_info)), m_output_texture_key(output_texture_key)
{
}

ShaderToyPassInfo & ShaderToyPassInfo::setDescriptorSetProxy(vkc::dsp::DescriptorSetProxy descriptor_set_proxy) noexcept
{
    auto set = descriptor_set_proxy.getSetIndex();
    auto position = std::ranges::lower_bound(m_ds_proxies, set, {}, &vkc::dsp::DescriptorSetProxy::getSetIndex);
    if (position != m_ds_proxies.end() and position->getSetIndex() == set) {
        *position = std::move(descriptor_set_proxy);
    } else {
        m_ds_proxies.insert(position, std::move(descriptor_set_proxy));
    }
    return *this;
}

ShaderToyInstanceInfo::ShaderToyInstanceInfo() noexcept : m_texture_set_id(next_texture_set_id())
{
}

ShaderToyInstanceInfo::ShaderToyInstanceInfo(Self && other) noexcept :
    m_texture_set_id(std::exchange(other.m_texture_set_id, next_texture_set_id())),
    m_textures(std::move(other.m_textures)), m_passes(std::move(other.m_passes))
{
}

ShaderToyInstanceInfo & ShaderToyInstanceInfo::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_texture_set_id = std::exchange(other.m_texture_set_id, next_texture_set_id());
    m_textures = std::move(other.m_textures);
    m_passes = std::move(other.m_passes);
    return *this;
}

TextureKey ShaderToyInstanceInfo::addTextureInfo(ShaderToyTextureInfo texture_info) noexcept
{
    auto texture_index = static_cast<uint32_t>(m_textures.size());
    m_textures.emplace_back(std::move(texture_info));
    return TextureKey {m_texture_set_id, texture_index};
}

ShaderToyInstanceInfo & ShaderToyInstanceInfo::addPassInfo(ShaderToyPassInfo pass_info) noexcept
{
    m_passes.emplace_back(std::move(pass_info));
    return *this;
}

std::error_code ShaderToyPass::create(vk::Device device, ShaderToyPassInfo pass_info, ImageListView output_images) noexcept
{
    m_output_texture_index = pass_info.getOutputTextureKey().getIndex();
    m_channels = std::move(pass_info.m_channels);
    m_ds_proxies = std::move(pass_info.m_ds_proxies);

    const auto & description = output_images.front().getDescription();
    const auto & extent = description.getExtent();
    vkc::AttachmentSetInfoBuilder attachment_set_builder;
    auto color_key = attachment_set_builder.addColorAttachment();
    auto attachment_set = attachment_set_builder.build();
    vkc::RenderTargetInfo target_info {attachment_set};
    target_info.setExtent({extent.width, extent.height}).setFormat(color_key, description.getFormat());
    m_render_targets.resize(output_images.size());
    for (std::size_t index = 0u; index < output_images.size(); ++index) {
        auto & target = m_render_targets[index];
        if (auto ec = target.build(target_info)) { return ec; }
        if (auto ec = target.setColorAttachment(color_key, output_images[index])) { return ec; }
    }

    vkc::DynamicRenderInfo render_info {attachment_set};
    render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore)
        .setEntryAttributes(color_key, vk::ImageLayout::eColorAttachmentOptimal);
    if (auto ec = m_dynamic_render.create(render_info)) { return ec; }

    vkc::ViewportStateInfo viewport_info;
    viewport_info.addViewport(0, 0, extent.width, extent.height).addScissor(0, 0, extent.width, extent.height);
    vkc::ColorBlendStateInfo blend_info {1u};
    vkc::GraphicsPipelineInfo pipeline_info;
    pipeline_info.setShaderProgramInfo(pass_info.m_shader_program_info)
        .setViewportStateInfo(viewport_info)
        .setColorBlendStateInfo(blend_info);
    return m_gfx_pipeline.create(device, pipeline_info, m_dynamic_render.makeScopeInfo());
}

std::error_code ShaderToyPass::build(
    vkc::CommandBufferProxy & cmd,
    const ShaderToyParams & params,
    uint32_t target_index,
    ImageViewListView channel_views) noexcept
{
    for (uint32_t channel = 0u; channel < channel_views.size(); ++channel) {
        if (m_channels[channel]) {
            m_ds_proxies.front().setImage(channel, channel_views[channel], vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
    for (auto & proxy : m_ds_proxies) {
        if (auto ec = proxy.updateIfDirty(cmd)) { return ec; }
    }
    auto & target = m_render_targets[target_index];
    auto extent = target.getMaxExtent();
    const ShaderToyParams::Resolution resolution {static_cast<float>(extent.width), static_cast<float>(extent.height), 1.0f};
    m_dynamic_render.begin(cmd, target);
    m_gfx_pipeline.bind(cmd);
    for (auto & proxy : m_ds_proxies) {
        proxy.bind(cmd, vk::PipelineBindPoint::eGraphics, m_gfx_pipeline.getPipelineLayout());
    }
    cmd.pushConstants<ShaderToyParams>(m_gfx_pipeline.getPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0u, params);
    cmd.pushConstants<float>(m_gfx_pipeline.getPipelineLayout(), vk::ShaderStageFlagBits::eFragment, offsetof(ShaderToyParams, m_resolution), resolution);
    cmd.draw(3u, 1u, 0u, 0u);
    m_dynamic_render.end(cmd);
    return {};
}

std::error_code ShaderToyInstance::create(vk::Device device, ShaderToyInstanceInfo shader_toy_instance_info) noexcept
{
    if (not m_textures.empty()) { return vkc::make_error_code(vkc::errc::already_created); }
    if (auto ec = validate_instance_info(shader_toy_instance_info)) { return ec; }
    m_textures.resize(shader_toy_instance_info.m_textures.size());
    for (std::size_t index = 0u; index < m_textures.size(); ++index) {
        auto & texture_info = shader_toy_instance_info.m_textures[index];
        auto & texture = m_textures[index];
        texture.m_images.resize(texture_info.m_images.size());
        for (std::size_t image_index = 0u; image_index < texture.m_images.size(); ++image_index) {
            auto & image = texture.m_images[image_index];
            image.m_image = std::move(texture_info.m_images[image_index]);
            image.m_layout = texture_info.m_layout;
            image.m_stage_flags = texture_info.m_stage_flags;
            image.m_access_flags = texture_info.m_access_flags;
            auto expected_view = image.m_image.createView(k_color_range, vk::ImageViewType::e2D);
            if (not expected_view) { return expected_view.error(); }
            image.m_view = std::move(*expected_view);
        }
    }
    m_passes.resize(shader_toy_instance_info.m_passes.size());
    for (std::size_t index = 0u; index < m_passes.size(); ++index) {
        auto & pass_info = shader_toy_instance_info.m_passes[index];
        auto & pass = m_passes[index];
        ShaderToyTextureInfo::ImageList output_images;
        for (const auto & image : m_textures[pass_info.getOutputTextureKey().getIndex()].m_images) { output_images.emplace_back(image.m_image); }
        if (auto ec = pass.create(device, std::move(pass_info), output_images)) { return ec; }
    }
    return {};
}

std::error_code ShaderToyInstance::build(vkc::CommandBufferProxy & cmd, const ShaderToyParams & params, uint64_t frame) noexcept
{
    for (auto & pass : m_passes) {
        if (auto ec = build_pass(cmd, pass, params, frame, m_textures)) { return ec; }
    }
    auto & images = m_textures[m_passes.back().getOutputTextureIndex()].m_images;
    auto & image = images[frame % images.size()];
    transition_image(cmd, image, vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead);
    return {};
}

const vkc::Image & ShaderToyInstance::getOutputImage(uint64_t frame) const noexcept
{
    const auto & images = m_textures[m_passes.back().getOutputTextureIndex()].m_images;
    return images[frame % images.size()].m_image;
}

} // namespace lcf::shader_toy

namespace {

uint64_t next_texture_set_id() noexcept
{
    static std::atomic_uint64_t texture_set_id {0u};
    return texture_set_id.fetch_add(1u, std::memory_order_relaxed);
}

std::error_code validate_instance_info(const ShaderToyInstanceInfo & shader_toy_instance_info) noexcept
{
    const auto invalid = std::make_error_code(std::errc::invalid_argument);
    const auto texture_infos = shader_toy_instance_info.viewTextureInfos();
    const auto pass_infos = shader_toy_instance_info.viewPassInfos();
    if (pass_infos.empty()) { return invalid; }
    constexpr auto k_no_writer = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> writers(texture_infos.size(), k_no_writer);
    for (std::size_t index = 0u; index < pass_infos.size(); ++index) {
        const auto & pass_info = pass_infos[index];
        auto output_texture_key = pass_info.getOutputTextureKey();
        if (not shader_toy_instance_info.containsTexture(output_texture_key) or pass_info.getShaderProgramInfo().viewStageInfos().empty()) { return invalid; }
        auto & writer = writers[output_texture_key.getIndex()];
        if (writer != k_no_writer) { return invalid; }
        writer = index;
        if (std::ranges::any_of(pass_info.viewChannelInfos(), [](const auto & channel_info_opt) { return channel_info_opt.has_value(); }) and
            (pass_info.viewDescriptorSetProxies().empty() or pass_info.viewDescriptorSetProxies().front().getSetIndex() != 0u)) { return invalid; }
    }
    for (std::size_t index = 0u; index < texture_infos.size(); ++index) {
        const auto images = texture_infos[index].viewImages();
        if (images.empty() or not images.front().handle()) { return invalid; }
        const auto & description = images.front().getDescription();
        for (const auto & image : images) {
            if (not image.handle() or image.getDescription() != description) { return invalid; }
        }
        if (images.size() == 2u and images[0].handle() == images[1].handle()) { return invalid; }
        if (writers[index] != k_no_writer and not (description.getUsageFlags() & vk::ImageUsageFlagBits::eColorAttachment)) { return invalid; }
    }
    for (std::size_t index = 0u; index < pass_infos.size(); ++index) {
        for (const auto & channel_info_opt : pass_infos[index].viewChannelInfos()) {
            if (not channel_info_opt) { continue; }
            const auto & channel_info = *channel_info_opt;
            auto texture_key = channel_info.getTextureKey();
            if (not shader_toy_instance_info.containsTexture(texture_key)) { return invalid; }
            const auto images = texture_infos[texture_key.getIndex()].viewImages();
            if (not (images.front().getDescription().getUsageFlags() & vk::ImageUsageFlagBits::eSampled)) { return invalid; }
            auto writer = writers[texture_key.getIndex()];
            if (channel_info.getFrame() == TextureFrame::ePrevious) {
                if (writer == k_no_writer or images.size() != 2u) { return invalid; }
            } else if (channel_info.getFrame() != TextureFrame::eCurrent or (writer != k_no_writer and writer >= index)) {
                return invalid;
            }
        }
    }
    const auto output_images = texture_infos[pass_infos.back().getOutputTextureKey().getIndex()].viewImages();
    if (output_images.size() != 2u or not (output_images.front().getDescription().getUsageFlags() & vk::ImageUsageFlagBits::eTransferSrc)) { return invalid; }
    return {};
}

void transition_image(vkc::CommandBufferProxy & cmd, auto & image, vk::ImageLayout layout,
    vk::PipelineStageFlags2 stage_flags, vk::AccessFlags2 access_flags) noexcept
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(image.m_image.handle()).setSubresourceRange(k_color_range)
        .setOldLayout(image.m_layout).setNewLayout(layout)
        .setSrcStageMask(image.m_stage_flags).setSrcAccessMask(image.m_access_flags)
        .setDstStageMask(stage_flags).setDstAccessMask(access_flags)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    vk::DependencyInfo dependency;
    dependency.setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(dependency);
    cmd.pinLease(image.m_image.lease());
    image.m_layout = layout;
    image.m_stage_flags = stage_flags;
    image.m_access_flags = access_flags;
}

std::error_code build_pass(
    vkc::CommandBufferProxy & cmd,
    ShaderToyPass & pass,
    const ShaderToyParams & params,
    uint64_t frame,
    auto & textures) noexcept
{
    std::array<vkc::ImageView, 4> channel_views;
    const auto channel_infos = pass.viewChannels();
    for (std::size_t channel_index = 0u; channel_index < channel_infos.size(); ++channel_index) {
        if (not channel_infos[channel_index]) { continue; }
        const auto & channel = *channel_infos[channel_index];
        auto & images = textures[channel.getTextureKey().getIndex()].m_images;
        auto index = frame % images.size();
        if (channel.getFrame() == TextureFrame::ePrevious) { index = (index + images.size() - 1u) % images.size(); }
        auto & image = images[index];
        transition_image(cmd, image, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead);
        channel_views[channel_index] = image.m_view;
    }
    auto & images = textures[pass.getOutputTextureIndex()].m_images;
    uint32_t target_index = static_cast<uint32_t>(frame % images.size());
    transition_image(cmd, images[target_index], vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite);
    return pass.build(cmd, params, target_index, channel_views);
}

}
