#pragma once

#include <vulkan/vulkan.hpp>
#include "DescriptorHeapLayout.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/Image.h"
#include "vk_core/sampler/Sampler.h"
#include <flat_map>
#include <system_error>
#include <variant>

namespace lcf::vkc {

class CommandBufferProxy;
class BufferView;
class ImageView;
class Sampler;

}


namespace lcf::vkc::dsh {

class DescriptorHeapAllocateInfo;
class DescriptorHeapAllocator;
class DescriptorHeapLayout;

class DescriptorHeapProxy
{
    using Self = DescriptorHeapProxy;
    struct AuthorityBufferBinding
    {
        vk::DescriptorType m_type;
        vkc::BufferView m_buffer_view;
    };
    struct AuthorityImageBinding
    {
        vk::DescriptorType m_type;
        vk::ImageLayout m_layout;
        vkc::ImageView m_image_view;
    };
    struct AuthoritySamplerBinding
    {
        vkc::Sampler m_sampler;
    };
    using AuthorityBufferMap = std::flat_map<uint32_t, AuthorityBufferBinding>;
    using AuthorityImageMap = std::flat_map<uint32_t, AuthorityImageBinding>;
    using AuthoritySamplerMap = std::flat_map<uint32_t, AuthoritySamplerBinding>;
public:
    ~DescriptorHeapProxy() noexcept = default;
    DescriptorHeapProxy() noexcept = default;
    DescriptorHeapProxy(const Self &) = delete;
    DescriptorHeapProxy(Self &&) = default;
    DescriptorHeapProxy & operator=(const Self &) = delete;
    DescriptorHeapProxy & operator=(Self &&) = default;
public:
    std::error_code create(DescriptorHeapAllocator & allocator, const DescriptorHeapLayout & layout) noexcept;
    Self & setBuffer(
        uint32_t index,
        vk::DescriptorType type,
        const vkc::BufferView & buffer_view) noexcept;
    Self & setImage(
        uint32_t index,
        vk::DescriptorType type,
        vk::ImageLayout image_layout,
        const vkc::ImageView & image_view) noexcept;
    Self & setSampler(uint32_t index, const vkc::Sampler & sampler) noexcept;
    std::error_code updateIfDirty(CommandBufferProxy & cmd) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
private:
    DescriptorHeapAllocator * m_allocator_p = nullptr;
    AuthorityBufferMap m_authority_buffers;
    AuthorityImageMap m_authority_images;
    AuthoritySamplerMap m_autority_samplers;
    uint64_t m_authority_resource_version = 0u;
    uint64_t m_authority_sampler_version = 0u;
    uint64_t m_uploaded_resource_version = 0u;
    uint64_t m_uploaded_sampler_version = 0u;
    DescriptorHeapLayout m_layout;
    vkc::Buffer m_resource_buffer;
    vkc::Buffer m_sampler_buffer;
};

} // namespace lcf::vkc::dsh
