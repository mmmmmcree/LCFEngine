#include "vk_core/memory/Image.h"
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

Image::Image(Memory && memory) noexcept :
    m_memory_rp(std::move(memory))
{
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

} // namespace lcf::vkc
