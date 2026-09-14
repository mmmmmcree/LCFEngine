#pragma once

#include "vk_core/memory/Buffer.h"
#include "DescriptorSetLayout.h"
#include <array>
#include <vector>
#include <variant>

namespace lcf::vkc {

class CommandBufferProxy;
class Buffer;
class ImageView;
class Sampler;

}

namespace lcf::vkc::dsb {

class DescriptorSetAllocator;

class DescriptorSetProxy
{
    using Self = DescriptorSetProxy;
    using DescriptorInfo = std::variant<vk::DescriptorAddressInfoEXT, vk::DescriptorImageInfo>;
    struct AuthorityBinding
    {
        DescriptorInfo m_descriptor_info;
        std::array<ResourceLease, 2> m_resource_leases;
    };
    using AuthorityBindingMap = std::vector<std::vector<AuthorityBinding>>;
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
    std::error_code updateIfDirty(CommandBufferProxy & cmd) noexcept;
    void bind(CommandBufferProxy & cmd, vk::PipelineBindPoint bind_point, vk::PipelineLayout pipeline_layout) noexcept;
private:
    DescriptorSetAllocator * m_allocator_p = nullptr;
    uint32_t m_set_index = 0u;
    uint32_t m_authority_version = 0u;
    uint32_t m_descriptor_set_buffer_version = 0u;
    DescriptorSetLayout m_layout;
    AuthorityBindingMap m_authority_bindings;
    vkc::Buffer m_descriptor_set_buffer;
};

} // namespace lcf::vkc::dsb
