#pragma once

#include <vulkan/vulkan.hpp>
#include <type_traits>
#include "resource_utils.h"

namespace lcf::vkc::details {

template <typename Handle>
requires std::is_same_v<Handle, vk::Image> or std::is_same_v<Handle, vk::Buffer>
class Memory;

} // namespace lcf::vkc::details

namespace lcf::vkc {

class Image
{
    using Self = Image;
    using Memory = details::Memory<vk::Image>;
public:
    ~Image() noexcept = default;
    Image() = default;
    explicit Image(Memory && memory) noexcept;
    Image(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    Image(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
private:
    ResourcePtr<Memory> m_memory_rp;
};

} // namespace lcf::vkc
