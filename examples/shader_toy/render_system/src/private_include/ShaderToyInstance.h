#pragma once

#include "vk_core/pipeline/graphics/GraphicsPipeline.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"
#include <vector>

namespace lcf::vkc {
class ShaderProgramInfo;
class CommandBufferProxy;
}

namespace lcf::shader_toy {

struct ShaderToyPassInfo
{
    const vkc::ShaderProgramInfo & m_shader_program_info;
    bool m_needs_history {false};
};

class ShaderToyPass
{
    using RenderTargetList = std::vector<vkc::RenderTarget>;
    using DescriptorSetProxyList = std::vector<vkc::dsp::DescriptorSetProxy>; 
public:
public:
    std::error_code create(const ShaderToyPassInfo & info) noexcept;
    void build(vkc::CommandBufferProxy & cmd) noexcept;
private:
    vkc::GraphicsPipeline m_gfx_pipeline;
    RenderTargetList m_render_targets;
    uint32_t m_target_index {0};
    
    DescriptorSetProxyList m_ds_proxies;
};

struct ShaderToyParams
{
    std::array<float, 3> m_resolution {0, 0, 1};
    float m_time {0.0f};
};

struct ShaderToyInstanceInfo
{
    using TextureList = std::vector<vkc::Image>;


};

class ShaderToyInstance
{
    using BufferPassList = std::vector<ShaderToyPass>;
    using ImageList = std::vector<vkc::Image>;
public:
    std::error_code create(const ShaderToyInstanceInfo & info) noexcept;
    void build(vkc::CommandBufferProxy & cmd, const ShaderToyParams params) noexcept;
private:
    ImageList m_textures;
    BufferPassList m_buffer_passes;
    ShaderToyPass m_image_pass;
};

}