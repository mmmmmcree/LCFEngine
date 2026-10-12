#pragma once

#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"
#include "vk_core/memory/Image.h"
#include "vk_core/pipeline/graphics/GraphicsPipeline.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/shader/info_structs.h"
#include "vk_core/queue/SubmissionToken.h"
#include "vk_core/sampler/Sampler.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <system_error>
#include <vector>

namespace lcf::vkc {

class CommandBufferProxy;

}

namespace lcf::shader_toy {

enum class ShaderToyResourceType : uint32_t
{
    eImage2D = 0u,
    eImage3D = 1u,
    eCubeMap = 2u,
    eBuffer = 3u,
    eCubeMapBuffer = 4u,
    eSelfDependentBuffer = 3u | 0x8000u,
    eSelfDependentCubeMapBuffer = 4u | 0x8000u,
};

enum class ShaderToyInputFrame
{
    eCurrent,
    ePrevious,
};

class ShaderToyInstanceInfo;
struct ShaderToyResourceReference;

class ShaderToyResourceId
{
    friend class ShaderToyInstanceInfo;
public:
    ShaderToyResourceId() noexcept = default;
    bool operator==(const ShaderToyResourceId &) const noexcept = default;
public:
    uint64_t getInstanceId() const noexcept { return m_instance_id; }
    uint32_t getIndex() const noexcept { return m_index; }
    ShaderToyResourceReference getFrameReference(ShaderToyInputFrame frame) const noexcept;
private:
    ShaderToyResourceId(uint64_t instance_id, uint32_t index) noexcept;
private:
    uint64_t m_instance_id = 0u;
    uint32_t m_index = 0u;
};

class ShaderToyPassId
{
    friend class ShaderToyInstanceInfo;
public:
    ShaderToyPassId() noexcept = default;
    bool operator==(const ShaderToyPassId &) const noexcept = default;
public:
    uint64_t getInstanceId() const noexcept { return m_instance_id; }
    uint32_t getIndex() const noexcept { return m_index; }
private:
    ShaderToyPassId(uint64_t instance_id, uint32_t index) noexcept;
    uint64_t m_instance_id = 0u;
    uint32_t m_index = 0u;
};

struct ShaderToyResourceReference
{
    ShaderToyResourceId m_resource_id;
    ShaderToyInputFrame m_frame = ShaderToyInputFrame::eCurrent;
};

struct ShaderToyImageResourceInfo
{
    using SubmissionTokenOptional = std::optional<vkc::SubmissionToken>;

    vkc::Image m_image;
    vkc::ImageView m_sampled_view;
    vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 m_stage_flags {};
    vk::AccessFlags2 m_access_flags {};
    SubmissionTokenOptional m_submission_token_opt;
};

struct ShaderToyChannelInfo
{
    ShaderToyResourceReference m_source;
    vkc::Sampler m_sampler;
};

using ShaderToyInputChannels = std::array<std::optional<ShaderToyChannelInfo>, 4>;

struct ShaderToyPassInfo
{
    using RenderTargetList = std::vector<vkc::RenderTarget>;

    vkc::ShaderProgramInfo m_shader_program_info;
    ShaderToyInputChannels m_input_channels;
    ShaderToyResourceId m_output_resource_id;
    vkc::dsp::DescriptorSetProxy m_image_descriptor_set_proxy;
    vkc::dsp::DescriptorSetProxy m_sampler_descriptor_set_proxy;
    vkc::GraphicsPipeline m_gfx_pipeline;
    RenderTargetList m_render_targets;
};

class ShaderToyInstanceInfo
{
    friend class ShaderToyInstance;
    using Self = ShaderToyInstanceInfo;
    using ResourceTypeList = std::vector<ShaderToyResourceType>;
public:
    using ImageResourceInfoList = std::vector<ShaderToyImageResourceInfo>;
    using ResourceInfoList = std::vector<ImageResourceInfoList>;
    using PassInfoList = std::vector<ShaderToyPassInfo>;
    using ResourceInfoListView = std::span<const ImageResourceInfoList>;
    using PassInfoListView = std::span<const ShaderToyPassInfo>;
public:
    ~ShaderToyInstanceInfo() noexcept = default;
    ShaderToyInstanceInfo() noexcept;
    ShaderToyInstanceInfo(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyInstanceInfo(Self && other) noexcept;
    Self & operator=(Self && other) noexcept;
public:
    ShaderToyResourceId addResource(vk::ArrayProxy<const ShaderToyImageResourceInfo> image_resource_infos) noexcept;
    ShaderToyPassId addPass(ShaderToyPassInfo pass_info) noexcept;
    Self & setOutputResource(ShaderToyResourceId resource_id) noexcept;
    std::error_code validate(vk::Device device) const noexcept;
    bool isValid() const noexcept { return m_is_valid; }
    uint64_t getInstanceId() const noexcept { return m_instance_id; }
    ShaderToyResourceId getOutputResourceId() const noexcept { return m_output_resource_id; }
    ResourceInfoListView viewResourceInfos() const noexcept { return m_resources; }
    PassInfoListView viewPassInfos() const noexcept { return m_passes; }
private:
    uint64_t m_instance_id = 0u;
    ResourceInfoList m_resources;
    PassInfoList m_passes;
    ShaderToyResourceId m_output_resource_id;
    mutable ResourceTypeList m_resource_types;
    mutable bool m_is_valid = false;
};

struct ShaderToyParams
{
    using Resolution = std::array<float, 3>;

    Resolution m_resolution {0, 0, 1};
    float m_time = 0.0f;
};

class ShaderToyInstance
{
    using Self = ShaderToyInstance;
    struct ImageResource
    {
        vkc::Image m_image;
        vkc::ImageView m_sampled_view;
        vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
        vk::PipelineStageFlags2 m_stage_flags {};
        vk::AccessFlags2 m_access_flags {};
    };
    using ImageResourceList = std::vector<ImageResource>;
    struct Resource
    {
        ImageResourceList m_image_resources;
        ShaderToyResourceType m_resource_type = ShaderToyResourceType::eImage2D;
    };
    struct InputChannel
    {
        uint32_t m_binding = 0u;
        ShaderToyResourceReference m_source;
    };
    struct Pass
    {
        using InputChannelList = std::vector<InputChannel>;
        using RenderTargetList = std::vector<vkc::RenderTarget>;

        InputChannelList m_input_channels;
        ShaderToyResourceId m_output_resource_id;
        vkc::dsp::DescriptorSetProxy m_image_descriptor_set_proxy;
        vkc::dsp::DescriptorSetProxy m_sampler_descriptor_set_proxy;
        vkc::GraphicsPipeline m_gfx_pipeline;
        RenderTargetList m_render_targets;
    };
    using ResourceList = std::vector<Resource>;
    using PassList = std::vector<Pass>;
public:
    ~ShaderToyInstance() noexcept = default;
    ShaderToyInstance() noexcept = default;
    ShaderToyInstance(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyInstance(Self && other) noexcept;
    Self & operator=(Self && other) noexcept;
public:
    std::error_code create(ShaderToyInstanceInfo && shader_toy_instance_info) noexcept;
    std::error_code build(vkc::CommandBufferProxy & cmd, const ShaderToyParams & params, uint64_t frame) noexcept;
    std::error_code setChannelSampler(ShaderToyPassId pass_id, uint32_t channel, vkc::Sampler sampler) noexcept;
    std::error_code setImageResource(ShaderToyResourceId resource_id,
        vk::ArrayProxy<const ShaderToyImageResourceInfo> image_resource_infos) noexcept;
    const vkc::Image & getOutputImage(uint64_t frame) const noexcept;
private:
    uint64_t m_instance_id = 0u;
    ResourceList m_resources;
    PassList m_passes;
    ShaderToyResourceId m_output_resource_id;
};

}
