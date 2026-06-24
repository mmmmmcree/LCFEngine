#include "vk_core/memory/Image.h"
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

Image::Image(Memory && memory) noexcept :
    m_memory_rp(std::move(memory))
{
}

} // namespace lcf::vkc
