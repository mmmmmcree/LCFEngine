# ShaderToyInstance 三步替换实现

本文保存可供 review 和提取的源码替换稿。源码块尚未应用到 examples 中；仓库现有 ShaderToyInstance.h、ShaderToyInstance.cpp 和 RenderSystem 不能视为已完成替换。本文的编译与运行验证状态见末尾。

## 已确定的接口和行为

- ShaderToyInstanceInfo 描述资源、channel 和 Pass 连接，ShaderToyInstance 是编译结果。
- ID 使用 instance id，跨 instance 或越界 ID 属于调用逻辑错误，在 cpp 中 assert。
- 导入纹理 Info 仅包含一张 vkc::Image、采样 view、初始状态和可选初始化 token。
- Buffer Info 保存具体 Vulkan 创建参数、轮换 image 数量和 self dependent 属性。
- ShaderToyResourceInfo 直接是 variant。运行 Resource 保留 ShaderToyResourceType。
- set 0 的 binding 0..3 对应 channel image，set 1 的 binding 0..3 对应 channel sampler。每个 Pass 两个 proxy。
- Pass 按添加顺序执行；create 检查当前帧输入的 producer 必须在 reader 之前。自身反馈读上一槽，下游读当前槽。
- create 验证 token 完成，完成前返回 resource_unavailable_try_again，保留 Info 供重试。create 不等待 GPU，也不接收 command buffer。
- 延迟创建的自依赖 Buffer 在第一次 build 清零首次读取的历史槽。普通全屏输出不清零，也没有可配置 clear value。
- image 状态逐物理槽追踪；layout 相同的重复只读不生成 barrier。首次 transition、读写冲突和输出供 blit 使用所需的 transition 仍保留。
- 公共 Info/ID 类型使用 ShaderToy 前缀；代码块没有注释；只有简单 return getter 内联。

## ResourceType 的完整范围

此枚举覆盖当前图的 GPU image 类别：2D、3D、cubemap 输入；2D/cubemap 渲染输出；两种输出各自的自依赖版本。低位存类别，0x8000 是自依赖位，低位类别不是互相独立的 flags，因此不启用 is_enum_flags_v。

视频、摄像头、键盘、音频频谱等数据由 RenderSystem 更新成具体 GPU image 后导入。ShaderToy 不负责这些数据的读取、解码或采集。Sound Pass 生成声音涉及另一个输出/执行接口，本文保持当前图的 image 输出范围。

cubemap 的六个面是每个物理 image 的六个 array layer，不是六个 frame slot。cubemap 输出逐面绘制，需要对应 fragment shader 声明 offset 16 的 uint face push constant，且其 push constant range 覆盖 20 字节。现有 cloud sea 的 16 字节 ShaderToyParams 和 shader 不改变；这项约定只用于新的 cubemap 输出 shader。

## 1. ShaderToyInstance.h

替换 examples/shader_toy/render_system/src/private_include/ShaderToyInstance.h：

```cpp
#pragma once

#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"
#include "vk_core/memory/Image.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/pipeline/graphics/DynamicRender.h"
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
#include <variant>
#include <vector>

namespace lcf::vkc {

class CommandBufferProxy;
class DeviceContext;

}

namespace lcf::shader_toy {

inline constexpr uint32_t k_shader_toy_self_dependency_bit = 0x8000u;

enum class ShaderToyResourceType : uint32_t
{
    eImage2D = 0u,
    eImage3D = 1u,
    eCubeMap = 2u,
    eBuffer = 3u,
    eCubeMapBuffer = 4u,
    eSelfDependentBuffer = 3u | k_shader_toy_self_dependency_bit,
    eSelfDependentCubeMapBuffer = 4u | k_shader_toy_self_dependency_bit,
};

struct ShaderToyParams
{
    using Resolution = std::array<float, 3>;

    Resolution m_resolution {0, 0, 1};
    float m_time = 0.0f;
};

struct ShaderToyResourceId
{
    bool operator==(const ShaderToyResourceId &) const noexcept = default;

    uint64_t m_instance_id = 0u;
    uint32_t m_resource_id = 0u;
};

struct ShaderToyPassId
{
    bool operator==(const ShaderToyPassId &) const noexcept = default;

    uint64_t m_instance_id = 0u;
    uint32_t m_pass_id = 0u;
};

struct ShaderToyImageResourceInfo
{
    vkc::Image m_image;
    vkc::ImageView m_sampled_view;
    vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 m_stage_flags {};
    vk::AccessFlags2 m_access_flags {};
    std::optional<vkc::SubmissionToken> m_submission_token_opt;
};

struct ShaderToyBufferResourceInfo
{
    vkc::ImageDescription m_image_description;
    vk::ImageCreateFlags m_image_create_flags {};
    vk::ImageViewType m_sampled_view_type = vk::ImageViewType::e2D;
    vkc::MemoryAllocationInfo m_allocation_info;
    uint32_t m_image_count = 1u;
    bool m_self_dependent = false;
};

using ShaderToyResourceInfo = std::variant<
    ShaderToyImageResourceInfo,
    ShaderToyBufferResourceInfo>;

struct ShaderToyChannelInfo
{
    ShaderToyResourceId m_resource_id;
    vkc::Sampler m_sampler;
};

using ShaderToyInputChannels = std::array<std::optional<ShaderToyChannelInfo>, 4>;

struct ShaderToyPassInfo
{
    vkc::ShaderProgramInfo m_shader_program_info;
    ShaderToyInputChannels m_input_channels;
    ShaderToyResourceId m_output_resource_id;
};

class ShaderToyInstanceInfo
{
    using Self = ShaderToyInstanceInfo;
public:
    using ResourceInfoList = std::vector<ShaderToyResourceInfo>;
    using PassInfoList = std::vector<ShaderToyPassInfo>;
    using ResourceInfoListView = std::span<const ShaderToyResourceInfo>;
    using PassInfoListView = std::span<const ShaderToyPassInfo>;
public:
    ~ShaderToyInstanceInfo() noexcept = default;
    ShaderToyInstanceInfo() noexcept;
    ShaderToyInstanceInfo(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    ShaderToyInstanceInfo(Self && other) noexcept;
    Self & operator=(Self && other) noexcept;
public:
    ShaderToyResourceId addResource(ShaderToyResourceInfo resource_info) noexcept;
    ShaderToyResourceId addImageResource(ShaderToyImageResourceInfo image_info) noexcept;
    ShaderToyResourceId addBufferResource(ShaderToyBufferResourceInfo buffer_info) noexcept;
    ShaderToyResourceId addReadWriteBufferResource(ShaderToyBufferResourceInfo buffer_info) noexcept;
    ShaderToyPassId addPass(ShaderToyPassInfo pass_info) noexcept;
    ShaderToyPassId addPass(
        vkc::ShaderProgramInfo program_info,
        ShaderToyInputChannels input_channels,
        ShaderToyResourceId output_resource_id) noexcept;
    Self & setOutputResource(ShaderToyResourceId resource_id) noexcept;
    uint64_t getInstanceId() const noexcept { return m_instance_id; }
    ShaderToyResourceId getOutputResourceId() const noexcept { return m_output_resource_id; }
    ResourceInfoListView viewResourceInfos() const noexcept { return m_resources; }
    PassInfoListView viewPassInfos() const noexcept { return m_passes; }
private:
    uint64_t m_instance_id = 0u;
    ResourceInfoList m_resources;
    PassInfoList m_passes;
    ShaderToyResourceId m_output_resource_id;
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

    struct Resource
    {
        std::vector<ImageResource> m_image_resources;
        ShaderToyResourceType m_resource_type = ShaderToyResourceType::eImage2D;
        bool m_needs_history_initialization = false;
    };

    struct Pass
    {
        using RenderTargetList = std::vector<vkc::RenderTarget>;

        ShaderToyInputChannels m_input_channels;
        ShaderToyResourceId m_output_resource_id;
        vkc::dsp::DescriptorSetProxy m_image_descriptor_set_proxy;
        vkc::dsp::DescriptorSetProxy m_sampler_descriptor_set_proxy;
        vkc::GraphicsPipeline m_gfx_pipeline;
        std::vector<RenderTargetList> m_render_targets;
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
    std::error_code create(
        const vkc::DeviceContext & device_context,
        vkc::dsp::DescriptorSetAllocator & descriptor_allocator,
        ShaderToyInstanceInfo && info) noexcept;
    std::error_code build(
        vkc::CommandBufferProxy & cmd,
        const ShaderToyParams & params,
        uint64_t frame) noexcept;
    std::error_code setChannelSampler(
        ShaderToyPassId pass_id,
        uint32_t channel,
        vkc::Sampler sampler) noexcept;
    ShaderToyResourceId getOutputResourceId() const noexcept { return m_output_resource_id; }
    const vkc::Image & getOutputImage(uint64_t frame) const noexcept;
private:
    uint64_t m_instance_id = 0u;
    ResourceList m_resources;
    PassList m_passes;
    ShaderToyResourceId m_output_resource_id;
};

}
```

## 2. ShaderToyInstance.cpp

替换 examples/shader_toy/render_system/src/ShaderToyInstance.cpp：

```cpp
#include "private_include/ShaderToyInstance.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/context/DeviceContext.h"
#include "vk_core/descriptor_set/info_structs.h"
#include "vk_core/error.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>

namespace {

using namespace lcf;
using namespace lcf::shader_toy;

uint64_t next_instance_id() noexcept;
void assert_resource_id(ShaderToyResourceId id, uint64_t instance_id, std::size_t count) noexcept;
bool is_self_dependent(ShaderToyResourceType type) noexcept;
ShaderToyResourceType make_resource_type(vk::ImageViewType view_type, bool buffer, bool self_dependent) noexcept;
std::error_code validate_instance_info(vk::Device device, const ShaderToyInstanceInfo & info) noexcept;
std::error_code create_resources(const vkc::DeviceContext & context, const ShaderToyInstanceInfo & info, auto & resources) noexcept;
std::error_code create_pass(vk::Device device, vkc::dsp::DescriptorSetAllocator & allocator,
    const ShaderToyPassInfo & info, auto & pass, const auto & output) noexcept;
uint32_t input_image_index(const auto & resource, ShaderToyResourceId input, ShaderToyResourceId output, uint64_t frame) noexcept;
bool has_write_access(vk::AccessFlags2 access) noexcept;
void transition_image(vkc::CommandBufferProxy & cmd, auto & image,
    vk::ImageLayout layout, vk::PipelineStageFlags2 stages, vk::AccessFlags2 access) noexcept;
void initialize_history(vkc::CommandBufferProxy & cmd, auto & resource, uint64_t frame) noexcept;
std::error_code prepare_descriptors(vkc::CommandBufferProxy & cmd, auto & pass, const auto & resources, uint64_t frame) noexcept;
void record_pass(vkc::CommandBufferProxy & cmd, auto & pass, auto & resources,
    const ShaderToyParams & params, uint64_t frame) noexcept;

}

namespace lcf::shader_toy {

static_assert(sizeof(ShaderToyParams) == 16u);

ShaderToyInstanceInfo::ShaderToyInstanceInfo() noexcept : m_instance_id(next_instance_id())
{
}

ShaderToyInstanceInfo::ShaderToyInstanceInfo(Self && other) noexcept :
    m_instance_id(std::exchange(other.m_instance_id, next_instance_id())),
    m_resources(std::move(other.m_resources)),
    m_passes(std::move(other.m_passes)),
    m_output_resource_id(std::exchange(other.m_output_resource_id, {}))
{
}

ShaderToyInstanceInfo & ShaderToyInstanceInfo::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_instance_id = std::exchange(other.m_instance_id, next_instance_id());
    m_resources = std::move(other.m_resources);
    m_passes = std::move(other.m_passes);
    m_output_resource_id = std::exchange(other.m_output_resource_id, {});
    return *this;
}

ShaderToyResourceId ShaderToyInstanceInfo::addResource(ShaderToyResourceInfo resource_info) noexcept
{
    assert(m_resources.size() < std::numeric_limits<uint32_t>::max());
    const auto index = static_cast<uint32_t>(m_resources.size());
    m_resources.emplace_back(std::move(resource_info));
    return {m_instance_id, index};
}

ShaderToyResourceId ShaderToyInstanceInfo::addImageResource(ShaderToyImageResourceInfo image_info) noexcept
{
    return this->addResource(std::move(image_info));
}

ShaderToyResourceId ShaderToyInstanceInfo::addBufferResource(ShaderToyBufferResourceInfo buffer_info) noexcept
{
    return this->addResource(std::move(buffer_info));
}

ShaderToyResourceId ShaderToyInstanceInfo::addReadWriteBufferResource(ShaderToyBufferResourceInfo buffer_info) noexcept
{
    buffer_info.m_self_dependent = true;
    buffer_info.m_image_count = std::max(2u, buffer_info.m_image_count);
    return this->addBufferResource(std::move(buffer_info));
}

ShaderToyPassId ShaderToyInstanceInfo::addPass(ShaderToyPassInfo pass_info) noexcept
{
    assert_resource_id(pass_info.m_output_resource_id, m_instance_id, m_resources.size());
    for (const auto & channel_opt : pass_info.m_input_channels) {
        if (channel_opt) { assert_resource_id(channel_opt->m_resource_id, m_instance_id, m_resources.size()); }
    }
    assert(m_passes.size() < std::numeric_limits<uint32_t>::max());
    const auto index = static_cast<uint32_t>(m_passes.size());
    m_passes.emplace_back(std::move(pass_info));
    return {m_instance_id, index};
}

ShaderToyPassId ShaderToyInstanceInfo::addPass(
    vkc::ShaderProgramInfo program_info,
    ShaderToyInputChannels input_channels,
    ShaderToyResourceId output_resource_id) noexcept
{
    return this->addPass({std::move(program_info), std::move(input_channels), output_resource_id});
}

ShaderToyInstanceInfo & ShaderToyInstanceInfo::setOutputResource(ShaderToyResourceId resource_id) noexcept
{
    assert_resource_id(resource_id, m_instance_id, m_resources.size());
    m_output_resource_id = resource_id;
    return *this;
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
    m_instance_id = std::exchange(other.m_instance_id, 0u);
    m_resources = std::move(other.m_resources);
    m_passes = std::move(other.m_passes);
    m_output_resource_id = std::exchange(other.m_output_resource_id, {});
    return *this;
}

std::error_code ShaderToyInstance::create(
    const vkc::DeviceContext & device_context,
    vkc::dsp::DescriptorSetAllocator & descriptor_allocator,
    ShaderToyInstanceInfo && info) noexcept
{
    if (m_instance_id != 0u) { return vkc::make_error_code(vkc::errc::already_created); }
    if (auto ec = validate_instance_info(device_context.getDevice(), info)) { return ec; }

    Self candidate;
    candidate.m_instance_id = info.getInstanceId();
    candidate.m_output_resource_id = info.getOutputResourceId();
    if (auto ec = create_resources(device_context, info, candidate.m_resources)) { return ec; }

    const auto pass_infos = info.viewPassInfos();
    candidate.m_passes.resize(pass_infos.size());
    for (std::size_t index = 0u; index < pass_infos.size(); ++index) {
        const auto & pass_info = pass_infos[index];
        auto & output = candidate.m_resources[pass_info.m_output_resource_id.m_resource_id];
        if (auto ec = create_pass(device_context.getDevice(), descriptor_allocator,
            pass_info, candidate.m_passes[index], output)) { return ec; }
        for (const auto & channel_opt : pass_info.m_input_channels) {
            if (channel_opt and channel_opt->m_resource_id == pass_info.m_output_resource_id) {
                output.m_needs_history_initialization = true;
            }
        }
    }

    *this = std::move(candidate);
    info = ShaderToyInstanceInfo {};
    return {};
}

std::error_code ShaderToyInstance::build(
    vkc::CommandBufferProxy & cmd,
    const ShaderToyParams & params,
    uint64_t frame) noexcept
{
    assert(m_instance_id != 0u);
    for (auto & pass : m_passes) {
        if (auto ec = prepare_descriptors(cmd, pass, m_resources, frame)) { return ec; }
    }
    for (auto & resource : m_resources) {
        if (resource.m_needs_history_initialization) { initialize_history(cmd, resource, frame); }
    }
    for (auto & pass : m_passes) { record_pass(cmd, pass, m_resources, params, frame); }

    auto & images = m_resources[m_output_resource_id.m_resource_id].m_image_resources;
    transition_image(cmd, images[frame % images.size()], vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead);
    return {};
}

std::error_code ShaderToyInstance::setChannelSampler(
    ShaderToyPassId pass_id,
    uint32_t channel,
    vkc::Sampler sampler) noexcept
{
    assert(m_instance_id != 0u and pass_id.m_instance_id == m_instance_id);
    assert(pass_id.m_pass_id < m_passes.size() and channel < 4u);
    auto & pass = m_passes[pass_id.m_pass_id];
    assert(pass.m_input_channels[channel].has_value());
    if (not sampler.handle()) { return std::make_error_code(std::errc::invalid_argument); }
    pass.m_input_channels[channel]->m_sampler = std::move(sampler);
    pass.m_sampler_descriptor_set_proxy.setSampler(channel, pass.m_input_channels[channel]->m_sampler);
    return {};
}

const vkc::Image & ShaderToyInstance::getOutputImage(uint64_t frame) const noexcept
{
    assert(m_instance_id != 0u);
    const auto & images = m_resources[m_output_resource_id.m_resource_id].m_image_resources;
    return images[frame % images.size()].m_image;
}

}

namespace {

uint64_t next_instance_id() noexcept
{
    static std::atomic_uint64_t instance_id {1u};
    return instance_id.fetch_add(1u, std::memory_order_relaxed);
}

void assert_resource_id(ShaderToyResourceId id, uint64_t instance_id, std::size_t count) noexcept
{
    assert(id.m_instance_id != 0u and id.m_instance_id == instance_id);
    assert(id.m_resource_id < count);
    static_cast<void>(id);
    static_cast<void>(instance_id);
    static_cast<void>(count);
}

bool is_self_dependent(ShaderToyResourceType type) noexcept
{
    return (std::to_underlying(type) & k_shader_toy_self_dependency_bit) != 0u;
}

ShaderToyResourceType make_resource_type(vk::ImageViewType view_type, bool buffer, bool self_dependent) noexcept
{
    if (buffer) {
        if (view_type == vk::ImageViewType::eCube) {
            return self_dependent ? ShaderToyResourceType::eSelfDependentCubeMapBuffer : ShaderToyResourceType::eCubeMapBuffer;
        }
        return self_dependent ? ShaderToyResourceType::eSelfDependentBuffer : ShaderToyResourceType::eBuffer;
    }
    if (view_type == vk::ImageViewType::eCube) { return ShaderToyResourceType::eCubeMap; }
    if (view_type == vk::ImageViewType::e3D) { return ShaderToyResourceType::eImage3D; }
    return ShaderToyResourceType::eImage2D;
}

std::error_code validate_instance_info(vk::Device device, const ShaderToyInstanceInfo & info) noexcept
{
    const auto invalid = std::make_error_code(std::errc::invalid_argument);
    const auto resources = info.viewResourceInfos();
    const auto passes = info.viewPassInfos();
    if (not device or resources.empty() or passes.empty()) { return invalid; }
    assert_resource_id(info.getOutputResourceId(), info.getInstanceId(), resources.size());
    constexpr auto k_no_writer = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> writers(resources.size(), k_no_writer);

    for (std::size_t index = 0u; index < resources.size(); ++index) {
        const auto & resource = resources[index];
        if (const auto * image_info_p = std::get_if<ShaderToyImageResourceInfo>(&resource)) {
            if (not image_info_p->m_image.handle() or not image_info_p->m_sampled_view.handle()) { return invalid; }
            const auto & image = image_info_p->m_image;
            const auto & view = image_info_p->m_sampled_view;
            const auto view_type = view.getViewType();
            if (view_type != vk::ImageViewType::e2D and view_type != vk::ImageViewType::e3D and
                view_type != vk::ImageViewType::eCube) { return std::make_error_code(std::errc::operation_not_supported); }
            if (view.makeInfo().image != image.handle() or
                not (image.getDescription().getUsageFlags() & vk::ImageUsageFlagBits::eSampled) or
                view.getRange().aspectMask != vk::ImageAspectFlagBits::eColor or
                image_info_p->m_layout == vk::ImageLayout::eUndefined) { return invalid; }
            for (std::size_t previous = 0u; previous < index; ++previous) {
                const auto * other_p = std::get_if<ShaderToyImageResourceInfo>(&resources[previous]);
                if (other_p and other_p->m_image.handle() == image.handle()) { return invalid; }
            }
            if (image_info_p->m_submission_token_opt) {
                const auto & token = *image_info_p->m_submission_token_opt;
                if (not token.semaphore) { return invalid; }
                try {
                    if (device.getSemaphoreCounterValue(token.semaphore) < token.value) {
                        return std::make_error_code(std::errc::resource_unavailable_try_again);
                    }
                } catch (const vk::SystemError & error) {
                    return error.code();
                }
            }
        } else {
            const auto & buffer_info = std::get<ShaderToyBufferResourceInfo>(resource);
            const auto & description = buffer_info.m_image_description;
            const auto & extent = description.getExtent();
            const auto usage = description.getUsageFlags();
            if (buffer_info.m_image_count == 0u or (buffer_info.m_self_dependent and buffer_info.m_image_count < 2u) or
                extent.width == 0u or extent.height == 0u or extent.depth != 1u or
                description.getType() != vk::ImageType::e2D or description.getFormat() == vk::Format::eUndefined or
                description.getSampleCount() != vk::SampleCountFlagBits::e1 or description.getMipLevelCount() != 1u or
                not (usage & vk::ImageUsageFlagBits::eColorAttachment) or not (usage & vk::ImageUsageFlagBits::eSampled)) { return invalid; }
            if (buffer_info.m_sampled_view_type == vk::ImageViewType::e2D) {
                if (description.getArrayLayerCount() != 1u) { return invalid; }
            } else if (buffer_info.m_sampled_view_type == vk::ImageViewType::eCube) {
                if (description.getArrayLayerCount() != 6u or extent.width != extent.height or
                    not (buffer_info.m_image_create_flags & vk::ImageCreateFlagBits::eCubeCompatible)) { return invalid; }
            } else {
                return std::make_error_code(std::errc::operation_not_supported);
            }
        }
    }

    for (std::size_t index = 0u; index < passes.size(); ++index) {
        const auto & pass = passes[index];
        assert_resource_id(pass.m_output_resource_id, info.getInstanceId(), resources.size());
        const auto * output_info_p = std::get_if<ShaderToyBufferResourceInfo>(&resources[pass.m_output_resource_id.m_resource_id]);
        if (not output_info_p) { return invalid; }
        auto & writer = writers[pass.m_output_resource_id.m_resource_id];
        if (writer != k_no_writer) { return invalid; }
        writer = index;

        bool has_vertex = false;
        bool has_fragment = false;
        const uint32_t required_push_size = sizeof(ShaderToyParams) +
            (output_info_p->m_sampled_view_type == vk::ImageViewType::eCube ? sizeof(uint32_t) : 0u);
        for (const auto & stage : pass.m_shader_program_info.viewStageInfos()) {
            if (stage.getStage() == vk::ShaderStageFlagBits::eVertex) { has_vertex = true; }
            if (stage.getStage() == vk::ShaderStageFlagBits::eFragment) {
                has_fragment = true;
                if (std::ranges::none_of(stage.getPushConstantRanges(), [required_push_size](const auto & range) {
                    return range.offset == 0u and range.size >= required_push_size;
                })) { return invalid; }
            }
        }
        if (not has_vertex or not has_fragment) { return invalid; }
    }

    for (std::size_t index = 0u; index < resources.size(); ++index) {
        if (std::holds_alternative<ShaderToyBufferResourceInfo>(resources[index]) and writers[index] == k_no_writer) { return invalid; }
    }
    for (std::size_t index = 0u; index < passes.size(); ++index) {
        const auto & pass = passes[index];
        for (const auto & channel_opt : pass.m_input_channels) {
            if (not channel_opt) { continue; }
            assert_resource_id(channel_opt->m_resource_id, info.getInstanceId(), resources.size());
            if (not channel_opt->m_sampler.handle()) { return invalid; }
            const auto writer = writers[channel_opt->m_resource_id.m_resource_id];
            if (channel_opt->m_resource_id == pass.m_output_resource_id) {
                const auto & buffer_info = std::get<ShaderToyBufferResourceInfo>(resources[pass.m_output_resource_id.m_resource_id]);
                if (not buffer_info.m_self_dependent) { return invalid; }
            } else if (writer != k_no_writer and writer >= index) {
                return invalid;
            }
        }
    }
    const auto * final_info_p = std::get_if<ShaderToyBufferResourceInfo>(&resources[info.getOutputResourceId().m_resource_id]);
    if (not final_info_p or not (final_info_p->m_image_description.getUsageFlags() & vk::ImageUsageFlagBits::eTransferSrc)) { return invalid; }
    return {};
}

std::error_code create_resources(
    const vkc::DeviceContext & context,
    const ShaderToyInstanceInfo & info,
    auto & resources) noexcept
{
    const auto resource_infos = info.viewResourceInfos();
    resources.resize(resource_infos.size());
    for (std::size_t index = 0u; index < resource_infos.size(); ++index) {
        auto & resource = resources[index];
        if (const auto * image_info_p = std::get_if<ShaderToyImageResourceInfo>(&resource_infos[index])) {
            resource.m_resource_type = make_resource_type(image_info_p->m_sampled_view.getViewType(), false, false);
            resource.m_image_resources.push_back({
                image_info_p->m_image, image_info_p->m_sampled_view,
                image_info_p->m_layout, image_info_p->m_stage_flags, image_info_p->m_access_flags});
        } else {
            const auto & buffer_info = std::get<ShaderToyBufferResourceInfo>(resource_infos[index]);
            const auto & description = buffer_info.m_image_description;
            resource.m_resource_type = make_resource_type(buffer_info.m_sampled_view_type, true, buffer_info.m_self_dependent);
            resource.m_image_resources.resize(buffer_info.m_image_count);
            auto usage = description.getUsageFlags();
            if (buffer_info.m_self_dependent) { usage |= vk::ImageUsageFlagBits::eTransferDst; }
            vk::ImageCreateInfo create_info;
            create_info.setFlags(buffer_info.m_image_create_flags)
                .setImageType(description.getType()).setFormat(description.getFormat())
                .setExtent(description.getExtent()).setMipLevels(description.getMipLevelCount())
                .setArrayLayers(description.getArrayLayerCount()).setSamples(description.getSampleCount())
                .setTiling(vk::ImageTiling::eOptimal).setUsage(usage)
                .setSharingMode(vk::SharingMode::eExclusive).setInitialLayout(vk::ImageLayout::eUndefined);
            const vk::ImageSubresourceRange range {
                vk::ImageAspectFlagBits::eColor, 0u, description.getMipLevelCount(),
                0u, description.getArrayLayerCount()};
            for (auto & image : resource.m_image_resources) {
                if (auto ec = image.m_image.create(context.getMemoryAllocator(), create_info, buffer_info.m_allocation_info)) { return ec; }
                auto expected_view = image.m_image.createView(range, buffer_info.m_sampled_view_type);
                if (not expected_view) { return expected_view.error(); }
                image.m_sampled_view = std::move(*expected_view);
            }
        }
    }
    return {};
}

std::error_code create_pass(
    vk::Device device,
    vkc::dsp::DescriptorSetAllocator & allocator,
    const ShaderToyPassInfo & info,
    auto & pass,
    const auto & output) noexcept
{
    pass.m_input_channels = info.m_input_channels;
    pass.m_output_resource_id = info.m_output_resource_id;
    vkc::DescriptorSetLayoutInfo image_layout_info;
    vkc::DescriptorSetLayoutInfo sampler_layout_info;
    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        const uint32_t descriptor_count = pass.m_input_channels[channel] ? 1u : 0u;
        image_layout_info.addBindingInfo(vk::DescriptorType::eSampledImage, descriptor_count, vk::ShaderStageFlagBits::eFragment);
        sampler_layout_info.addBindingInfo(vk::DescriptorType::eSampler, descriptor_count, vk::ShaderStageFlagBits::eFragment);
    }
    vkc::dsp::DescriptorSetLayout image_layout;
    vkc::dsp::DescriptorSetLayout sampler_layout;
    if (auto ec = image_layout.create(device, image_layout_info)) { return ec; }
    if (auto ec = sampler_layout.create(device, sampler_layout_info)) { return ec; }
    if (auto ec = pass.m_image_descriptor_set_proxy.create(allocator, image_layout)) { return ec; }
    if (auto ec = pass.m_sampler_descriptor_set_proxy.create(allocator, sampler_layout)) { return ec; }
    pass.m_image_descriptor_set_proxy.setSetIndex(0u);
    pass.m_sampler_descriptor_set_proxy.setSetIndex(1u);

    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        const auto & channel_opt = pass.m_input_channels[channel];
        if (channel_opt) { pass.m_sampler_descriptor_set_proxy.setSampler(channel, channel_opt->m_sampler); }
    }

    const auto & description = output.m_image_resources.front().m_image.getDescription();
    const auto & extent = description.getExtent();
    vkc::AttachmentSetInfoBuilder attachment_builder;
    const auto color_key = attachment_builder.addColorAttachment();
    auto attachments = attachment_builder.build();
    vkc::RenderTargetInfo target_info {attachments};
    target_info.setExtent({extent.width, extent.height}).setFormat(color_key, description.getFormat());
    pass.m_render_targets.resize(output.m_image_resources.size());
    for (std::size_t slot = 0u; slot < output.m_image_resources.size(); ++slot) {
        auto & targets = pass.m_render_targets[slot];
        targets.resize(description.getArrayLayerCount());
        for (uint32_t layer = 0u; layer < targets.size(); ++layer) {
            if (auto ec = targets[layer].build(target_info)) { return ec; }
            if (auto ec = targets[layer].setColorAttachment(color_key, output.m_image_resources[slot].m_image, 0u, layer)) { return ec; }
        }
    }

    auto program_info = info.m_shader_program_info;
    program_info.addDescriptorSetLayout(0u, image_layout.handle())
        .addDescriptorSetLayout(1u, sampler_layout.handle());
    vkc::ViewportStateInfo viewport_info;
    viewport_info.addViewport(0, 0, extent.width, extent.height)
        .addScissor(0, 0, extent.width, extent.height);
    vkc::ColorBlendStateInfo blend_info {1u};
    vkc::GraphicsPipelineInfo pipeline_info;
    pipeline_info.setShaderProgramInfo(program_info).setViewportStateInfo(viewport_info).setColorBlendStateInfo(blend_info);
    const std::array color_formats {description.getFormat()};
    const vkc::DynamicRenderScopeInfo scope_info {
        color_formats, vk::Format::eUndefined, vk::Format::eUndefined, 0u};
    return pass.m_gfx_pipeline.create(device, pipeline_info, scope_info);
}

uint32_t input_image_index(
    const auto & resource,
    ShaderToyResourceId input,
    ShaderToyResourceId output,
    uint64_t frame) noexcept
{
    const auto count = resource.m_image_resources.size();
    const auto current = frame % count;
    return static_cast<uint32_t>(
        input == output and is_self_dependent(resource.m_resource_type) ? (current + count - 1u) % count : current);
}

bool has_write_access(vk::AccessFlags2 access) noexcept
{
    constexpr auto k_writes = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderWrite |
        vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eColorAttachmentWrite |
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eTransferWrite |
        vk::AccessFlagBits2::eHostWrite;
    return bool(access & k_writes);
}

void transition_image(
    vkc::CommandBufferProxy & cmd,
    auto & image,
    vk::ImageLayout layout,
    vk::PipelineStageFlags2 stages,
    vk::AccessFlags2 access) noexcept
{
    cmd.pinLease(image.m_image.lease());
    if (image.m_layout == layout and not has_write_access(image.m_access_flags) and not has_write_access(access)) {
        image.m_stage_flags |= stages;
        image.m_access_flags |= access;
        return;
    }
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(image.m_image.handle()).setSubresourceRange(image.m_sampled_view.getRange())
        .setOldLayout(image.m_layout).setNewLayout(layout)
        .setSrcStageMask(image.m_stage_flags).setSrcAccessMask(image.m_access_flags)
        .setDstStageMask(stages).setDstAccessMask(access)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    vk::DependencyInfo dependency;
    dependency.setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(dependency);
    image.m_layout = layout;
    image.m_stage_flags = stages;
    image.m_access_flags = access;
}

void initialize_history(vkc::CommandBufferProxy & cmd, auto & resource, uint64_t frame) noexcept
{
    auto & images = resource.m_image_resources;
    const auto previous = (frame % images.size() + images.size() - 1u) % images.size();
    auto & image = images[previous];
    transition_image(cmd, image, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);
    cmd.clearColorImage(image.m_image.handle(), vk::ImageLayout::eTransferDstOptimal,
        vk::ClearColorValue {std::array<float, 4> {0, 0, 0, 0}}, image.m_sampled_view.getRange());
    resource.m_needs_history_initialization = false;
}

std::error_code prepare_descriptors(
    vkc::CommandBufferProxy & cmd,
    auto & pass,
    const auto & resources,
    uint64_t frame) noexcept
{
    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        const auto & channel_opt = pass.m_input_channels[channel];
        if (not channel_opt) { continue; }
        const auto & resource = resources[channel_opt->m_resource_id.m_resource_id];
        const auto & view = resource.m_image_resources[
            input_image_index(resource, channel_opt->m_resource_id, pass.m_output_resource_id, frame)].m_sampled_view;
        pass.m_image_descriptor_set_proxy.setImage(channel, view, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    if (auto ec = pass.m_image_descriptor_set_proxy.updateIfDirty(cmd)) { return ec; }
    return pass.m_sampler_descriptor_set_proxy.updateIfDirty(cmd);
}

void record_pass(
    vkc::CommandBufferProxy & cmd,
    auto & pass,
    auto & resources,
    const ShaderToyParams & params,
    uint64_t frame) noexcept
{
    for (const auto & channel_opt : pass.m_input_channels) {
        if (not channel_opt) { continue; }
        auto & resource = resources[channel_opt->m_resource_id.m_resource_id];
        auto & image = resource.m_image_resources[
            input_image_index(resource, channel_opt->m_resource_id, pass.m_output_resource_id, frame)];
        transition_image(cmd, image, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead);
    }

    auto & output = resources[pass.m_output_resource_id.m_resource_id];
    const auto slot = frame % output.m_image_resources.size();
    transition_image(cmd, output.m_image_resources[slot], vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite);
    pass.m_gfx_pipeline.bind(cmd);
    const auto pipeline_layout = pass.m_gfx_pipeline.getPipelineLayout();
    pass.m_image_descriptor_set_proxy.bind(cmd, vk::PipelineBindPoint::eGraphics, pipeline_layout);
    pass.m_sampler_descriptor_set_proxy.bind(cmd, vk::PipelineBindPoint::eGraphics, pipeline_layout);

    const auto & targets = pass.m_render_targets[slot];
    for (uint32_t layer = 0u; layer < targets.size(); ++layer) {
        const auto & target = targets[layer];
        const auto & attachment_view = target.viewAttachments().front().getImageView();
        vk::RenderingAttachmentInfo color_attachment;
        color_attachment.setImageView(attachment_view).setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eDontCare).setStoreOp(vk::AttachmentStoreOp::eStore);
        vk::RenderingInfo rendering_info;
        rendering_info.setRenderArea(target.getRenderArea()).setLayerCount(1u).setColorAttachments(color_attachment);
        cmd.beginRendering(rendering_info);

        auto pass_params = params;
        const auto & extent = target.getMaxExtent();
        pass_params.m_resolution = {static_cast<float>(extent.width), static_cast<float>(extent.height), 1.0f};
        cmd.pushConstants<ShaderToyParams>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0u, pass_params);
        if (targets.size() == 6u) {
            cmd.pushConstants<uint32_t>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, sizeof(ShaderToyParams), layer);
        }
        cmd.draw(3u, 1u, 0u, 0u);
        cmd.endRendering();
    }
}

}

```

## 3. RenderSystem

后续完整替换写在本节。

## 验证状态

文档中的替换稿尚未编译、链接或运行。当前 up in the cloud sea 运行效果尚未验证。
