#pragma once

#include "details/info_structs.h"
#include "vk_core/utils/ResourceHandle.h"
#include "DescriptorSetLayout.h"
#include <variant>

namespace lcf::vkc {

class CommandBufferProxy;
class Buffer;
class ImageView;
class Sampler;

} 

namespace lcf::vkc::dsp {

class DescriptorSetAllocator;

class DescriptorSetProxy
{
    using Self = DescriptorSetProxy;
    using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
    struct AuthorityBinding
    {
        DescriptorInfo m_descriptor_info;
        ResourceLease m_lease;
    };
    struct DescriptorSet
    {
        vk::DescriptorPool m_pool;
        vk::DescriptorPoolCreateFlags m_pool_key;
        vk::DescriptorSet m_descriptor_set;
        uint32_t m_version = 0u;
    };
    using AuthorityBindingMap = std::vector<std::vector<AuthorityBinding>>;
    using DescriptorSetList = std::vector<DescriptorSet>;
public:
    ~DescriptorSetProxy() noexcept;
    DescriptorSetProxy() noexcept = default;
    DescriptorSetProxy(const DescriptorSetProxy &) = delete;
    DescriptorSetProxy(DescriptorSetProxy &&) = default;
    DescriptorSetProxy & operator=(const DescriptorSetProxy &) = delete;
    DescriptorSetProxy & operator=(DescriptorSetProxy &&) = default;
public:
    std::error_code create(DescriptorSetAllocator & allocator, const DescriptorSetLayout & layout) noexcept;
    Self & setSetIndex(uint32_t set_index) noexcept { m_set_index = set_index; return *this; }
    const uint32_t & getSetIndex() const noexcept { return m_set_index; }
    Self & setBuffer(uint32_t binding, uint32_t array_index, const vkc::Buffer & buffer, vk::DeviceSize offset = 0u, vk::DeviceSize range = vk::WholeSize) noexcept;
    Self & setBuffer(uint32_t binding, const vkc::Buffer & buffer, vk::DeviceSize offset = 0u, vk::DeviceSize range = vk::WholeSize) noexcept;
    Self & setImage(uint32_t binding, uint32_t array_index, const vkc::ImageView & image_view, vk::ImageLayout image_layout) noexcept;
    Self & setImage(uint32_t binding, const vkc::ImageView & image_view, vk::ImageLayout image_layout) noexcept;
    Self & setSampler(uint32_t binding, uint32_t array_index, const vkc::Sampler & sampler) noexcept;
    Self & setSampler(uint32_t binding, const vkc::Sampler & sampler) noexcept;
    std::error_code commitUpdate(CommandBufferProxy & cmd) noexcept;
    void bind(CommandBufferProxy & cmd, vk::PipelineBindPoint bind_point, vk::PipelineLayout pipeline_layout) noexcept;
private:
    DescriptorSetAllocator * m_allocator_p = nullptr;
    uint32_t m_set_index = 0u;
    uint32_t m_version = 0u;
    DescriptorSetLayout m_layout;
    AuthorityBindingMap m_authority_bindings;
    DescriptorSetList m_in_use_descriptor_sets;
    DescriptorSetList m_available_descriptor_sets;
};

} // namespace lcf::vkc::dsb