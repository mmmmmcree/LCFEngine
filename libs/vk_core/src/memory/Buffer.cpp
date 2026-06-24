#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

Buffer::Buffer(Memory && memory, vk::DeviceAddress device_address) noexcept :
    m_memory_rp(std::move(memory)),
    m_device_address(device_address)
{
}

} // namespace lcf::vkc
