#pragma once

#include "vk_core/descriptor_set/pool/details/info_structs.h"

namespace lcf::vkc {

class CommandBufferProxy;
class Buffer;
class ImageView;
class Sampler;

} 

namespace lcf::vkc::dsp {

class DescriptorSetProxy
{
    using Self = DescriptorSetProxy;
public:

public:
    Self & setBuffer(uint32_t binding, const vkc::Buffer & buffer, vk::DeviceSize offset = 0u, vk::DeviceSize range = vk::WholeSize) noexcept;
    Self & setBuffer(uint32_t binding, uint32_t array_index, const vkc::Buffer & buffer, vk::DeviceSize offset = 0u, vk::DeviceSize range = vk::WholeSize) noexcept;
    Self & setImage(uint32_t binding, const vkc::ImageView & image_view, vk::ImageLayout image_layout) noexcept;
    Self & setImage(uint32_t binding, uint32_t array_index, const vkc::ImageView & image_view, vk::ImageLayout image_layout) noexcept;
    Self & setSampler(uint32_t binding, const vkc::Sampler & sampler) noexcept;
    Self & setSampler(uint32_t binding, uint32_t array_index, const vkc::Sampler & sampler) noexcept;
    std::error_code commitUpdate(CommandBufferProxy & cmd) noexcept;
private:

};

} // namespace lcf::vkc::dsb