#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include "vk_core/error.h"

namespace lcf::vkc::bs {

class InstanceCreateInfo;

struct InstanceCreateResult
{
    InstanceCreateResult() noexcept = default;
    InstanceCreateResult(vk::UniqueInstance instance, Warning warning = {}) noexcept :
        m_instance(std::move(instance)), m_warning({warning}) {}
    operator vk::UniqueInstance &&() && noexcept
    {
        return std::move(m_instance);
    }

    const Warning & getWarning() const noexcept { return m_warning; }

    vk::UniqueInstance m_instance;
    Warning m_warning;
};

std::expected<InstanceCreateResult, Error> create_instance(const InstanceCreateInfo & create_info) noexcept;

} // namespace lcf::vkc::bs
