#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/utils/concepts/structure_concepts.h"

namespace lcf::vkc::utils {

template <typename Root>
class ExtensionLinker
{
    using Self = ExtensionLinker<Root>;
public:
    ExtensionLinker() noexcept = default;
    ExtensionLinker(const Self &) noexcept = default;
    ExtensionLinker(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Extension>
    requires struct_extends_c<Extension, Root>
    Self & link(Extension & extension) noexcept
    {
        auto * extension_base = reinterpret_cast<vk::BaseOutStructure *>(&extension);
        extension_base->pNext = m_head;
        m_head = extension_base;
        return *this;
    }
    const void * getPNext() const noexcept { return m_head; }
private:
    vk::BaseOutStructure * m_head = nullptr;
};

} // namespace lcf::vkc::utils
