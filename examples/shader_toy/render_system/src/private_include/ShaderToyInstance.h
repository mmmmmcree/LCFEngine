#pragma once

#include "vk_core/pipeline/graphics/GraphicsPipeline.h"
#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/shader/info_structs.h"
#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace lcf::vkc {
class CommandBufferProxy;
}

namespace lcf::shader_toy {

enum class TextureFrame
{
    eCurrent,
    ePrevious,
};

class ShaderToyInstanceInfo;

class TextureKey
{
    friend class ShaderToyInstanceInfo;
public:
    uint32_t getIndex() const noexcept { return m_index; }
    bool validate(uint64_t texture_set_id) const noexcept { return m_texture_set_id == texture_set_id; }
private:
    TextureKey(uint64_t texture_set_id, uint32_t index) noexcept : m_texture_set_id(texture_set_id), m_index(index) {}
    uint64_t m_texture_set_id;
    uint32_t m_index;
};

class ShaderToyChannelInfo
{
public:
    ShaderToyChannelInfo(TextureKey texture_key, TextureFrame frame = TextureFrame::eCurrent) noexcept :
        m_texture_key(texture_key), m_frame(frame) {}
    TextureKey getTextureKey() const noexcept { return m_texture_key; }
    TextureFrame getFrame() const noexcept { return m_frame; }
private:
    TextureKey m_texture_key;
    TextureFrame m_frame;
};

class ShaderToyTextureInfo
{
    friend class ShaderToyInstance;
    using Self = ShaderToyTextureInfo;
public:
    using ImageList = std::vector<vkc::Image>;
    using ImagePair = std::array<vkc::Image, 2>;
    using ImageListView = std::span<const vkc::Image>;
public:
    explicit ShaderToyTextureInfo(vkc::Image image) noexcept;
    explicit ShaderToyTextureInfo(ImagePair images) noexcept;
    ShaderToyTextureInfo(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyTextureInfo(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    Self & setInitialState(vk::ImageLayout layout, vk::PipelineStageFlags2 stage_flags, vk::AccessFlags2 access_flags) noexcept;
    ImageListView viewImages() const noexcept { return m_images; }
private:
    ImageList m_images;
    vk::ImageLayout m_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::PipelineStageFlags2 m_stage_flags = vk::PipelineStageFlagBits2::eFragmentShader;
    vk::AccessFlags2 m_access_flags = vk::AccessFlagBits2::eShaderSampledRead;
};

class ShaderToyPassInfo
{
    friend class ShaderToyPass;
    using Self = ShaderToyPassInfo;
public:
    using ChannelInfoSlot = std::optional<ShaderToyChannelInfo>;
    using ChannelList = std::array<ChannelInfoSlot, 4>;
    using DescriptorSetProxyList = std::vector<vkc::dsp::DescriptorSetProxy>;
    using ChannelListView = std::span<const ChannelInfoSlot>;
    using DescriptorSetProxyListView = std::span<const vkc::dsp::DescriptorSetProxy>;
public:
    ShaderToyPassInfo(TextureKey output_texture_key, vkc::ShaderProgramInfo shader_program_info) noexcept;
    ShaderToyPassInfo(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyPassInfo(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    template <std::size_t Channel>
    requires (Channel < std::tuple_size_v<ChannelList>)
    Self & setChannelInfo(ShaderToyChannelInfo channel_info) noexcept
    {
        m_channels[Channel] = std::move(channel_info);
        return *this;
    }
    Self & setDescriptorSetProxy(vkc::dsp::DescriptorSetProxy descriptor_set_proxy) noexcept;
    const vkc::ShaderProgramInfo & getShaderProgramInfo() const noexcept { return m_shader_program_info; }
    TextureKey getOutputTextureKey() const noexcept { return m_output_texture_key; }
    ChannelListView viewChannelInfos() const noexcept { return m_channels; }
    DescriptorSetProxyListView viewDescriptorSetProxies() const noexcept { return m_ds_proxies; }
private:
    vkc::ShaderProgramInfo m_shader_program_info;
    TextureKey m_output_texture_key;
    ChannelList m_channels;
    DescriptorSetProxyList m_ds_proxies;
};

struct ShaderToyParams
{
    using Resolution = std::array<float, 3>;

    Resolution m_resolution {0, 0, 1};
    float m_time = 0.0f;
};

class ShaderToyPass
{
    using Self = ShaderToyPass;
    using RenderTargetList = std::vector<vkc::RenderTarget>;
    using ChannelList = ShaderToyPassInfo::ChannelList;
    using DescriptorSetProxyList = ShaderToyPassInfo::DescriptorSetProxyList;
    using ImageViewListView = std::span<const vkc::ImageView>;
    using ImageListView = std::span<const vkc::Image>;
    using ChannelListView = ShaderToyPassInfo::ChannelListView;
public:
    ~ShaderToyPass() noexcept = default;
    ShaderToyPass() noexcept = default;
    ShaderToyPass(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyPass(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, ShaderToyPassInfo pass_info, ImageListView output_images) noexcept;
    std::error_code build(vkc::CommandBufferProxy & cmd, const ShaderToyParams & params, uint32_t target_index, ImageViewListView channel_views) noexcept;
    uint32_t getOutputTextureIndex() const noexcept { return m_output_texture_index; }
    ChannelListView viewChannels() const noexcept { return m_channels; }
private:
    uint32_t m_output_texture_index = 0u;
    ChannelList m_channels;
    DescriptorSetProxyList m_ds_proxies;
    vkc::GraphicsPipeline m_gfx_pipeline;
    vkc::DynamicRender m_dynamic_render;
    RenderTargetList m_render_targets;
};

class ShaderToyInstanceInfo
{
    friend class ShaderToyInstance;
    using Self = ShaderToyInstanceInfo;
public:
    using TextureInfoList = std::vector<ShaderToyTextureInfo>;
    using PassInfoList = std::vector<ShaderToyPassInfo>;
    using TextureInfoListView = std::span<const ShaderToyTextureInfo>;
    using PassInfoListView = std::span<const ShaderToyPassInfo>;
public:
    ShaderToyInstanceInfo() noexcept;
    ShaderToyInstanceInfo(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyInstanceInfo(Self && other) noexcept;
    Self & operator=(Self && other) noexcept;
    TextureKey addTextureInfo(ShaderToyTextureInfo texture_info) noexcept;
    Self & addPassInfo(ShaderToyPassInfo pass_info) noexcept;
    TextureInfoListView viewTextureInfos() const noexcept { return m_textures; }
    PassInfoListView viewPassInfos() const noexcept { return m_passes; }
    bool containsTexture(TextureKey texture_key) const noexcept
    {
        return texture_key.validate(m_texture_set_id) and texture_key.getIndex() < m_textures.size();
    }
private:
    uint64_t m_texture_set_id;
    TextureInfoList m_textures;
    // Execution order; back() is the final Image pass.
    PassInfoList m_passes;
};

class ShaderToyInstance
{
    using Self = ShaderToyInstance;
    struct ImageResource
    {
        vkc::Image m_image;
        vkc::ImageView m_view;
        vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
        vk::PipelineStageFlags2 m_stage_flags;
        vk::AccessFlags2 m_access_flags;
    };
    struct TextureResource
    {
        using ImageResourceList = std::vector<ImageResource>;

        ImageResourceList m_images;
    };
    using TextureResourceList = std::vector<TextureResource>;
    using PassList = std::vector<ShaderToyPass>;
public:
    ~ShaderToyInstance() noexcept = default;
    ShaderToyInstance() noexcept = default;
    ShaderToyInstance(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyInstance(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, ShaderToyInstanceInfo shader_toy_instance_info) noexcept;
    std::error_code build(vkc::CommandBufferProxy & cmd, const ShaderToyParams & params, uint64_t frame) noexcept;
    const vkc::Image & getOutputImage(uint64_t frame) const noexcept;
private:
    TextureResourceList m_textures;
    PassList m_passes;
};

} // namespace lcf::shader_toy
