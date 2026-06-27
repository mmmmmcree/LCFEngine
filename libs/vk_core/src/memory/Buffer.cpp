#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

Buffer::Buffer(Memory && memory, vk::DeviceAddress device_address) noexcept :
    m_memory_rp(std::move(memory)),
    m_device_address(device_address)
{
}

Buffer::operator vk::Buffer() const noexcept
{
    return m_memory_rp->handle();
}

ResourceLease Buffer::lease() const noexcept
{
    return m_memory_rp.lease();
}

const vk::Buffer & Buffer::handle() const noexcept
{
    return m_memory_rp->handle();
}

} // namespace lcf::vkc
