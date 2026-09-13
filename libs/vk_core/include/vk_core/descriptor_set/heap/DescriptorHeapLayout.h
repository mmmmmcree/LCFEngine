#pragma once
#include <cstdint>

namespace lcf::vkc::dsh {

class DescriptorHeapLayout
{
    using Self  = DescriptorHeapLayout;
public:
    Self & setBufferCount(uint32_t count) noexcept { m_buffer_count = count; return *this; }
    Self & setImageCount(uint32_t count) noexcept { m_image_count = count; return *this; }
    Self & setSamplerCount(uint32_t count) noexcept { m_sampler_count = count; return *this; }
    uint32_t getBufferCount() const noexcept { return m_buffer_count; }
    uint32_t getImageCount() const noexcept { return m_image_count; }
    uint32_t getSamplerCount() const noexcept { return m_sampler_count; }
private:
    uint32_t m_buffer_count = 0u;
    uint32_t m_image_count = 0u;
    uint32_t m_sampler_count = 0u;
};

} // namespace lcf::vkc::dsh
