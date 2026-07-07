#include "vk_core/memory/Image.h"
#include "vk_core/memory/details/Memory.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"

namespace lcf::vkc {

std::error_code Image::create(
    const MemoryAllocator & allocator,
    const vk::ImageCreateInfo & image_info,
    const MemoryAllocationInfo & alloc_info) noexcept
{
    auto expected_memory = allocator.allocateImage(image_info, alloc_info);
    if (not expected_memory) { return expected_memory.error(); }
    m_memory_rp = std::move(expected_memory.value());
    m_device = allocator.getDevice();
    m_desc = image_info;
    return {};
}

Image::operator vk::Image() const noexcept
{
    return m_memory_rp->handle();
}

ResourceLease Image::lease() const noexcept
{
    return m_memory_rp.lease();
}

const vk::Image & Image::handle() const noexcept
{
    return m_memory_rp->handle();
}

constexpr uint64_t ImageDescription::hash(const Self &self) noexcept
{
    constexpr auto mix = [](uint64_t seed, uint64_t value) noexcept -> uint64_t {
        value += 0x9e3779b97f4a7c15ull;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
        value ^= value >> 31;
        return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
    };
    uint64_t h = 0ull;
    h = mix(h, static_cast<uint64_t>(self.m_type));
    h = mix(h, static_cast<uint64_t>(self.m_format));
    h = mix(h, static_cast<uint64_t>(self.m_sample_count));
    h = mix(h, static_cast<uint64_t>(static_cast<uint32_t>(self.m_usage_flags)));
    h = mix(h, static_cast<uint64_t>(self.m_extent.width));
    h = mix(h, static_cast<uint64_t>(self.m_extent.height));
    h = mix(h, static_cast<uint64_t>(self.m_extent.depth));
    h = mix(h, static_cast<uint64_t>(self.m_mip_level_count));
    h = mix(h, static_cast<uint64_t>(self.m_array_layer_count));
    return h;
}

} // namespace lcf::vkc
