#pragma once

#include <vulkan/vulkan.hpp>
#include <system_error>
#include "vk_core/sampler/info_structs.h"
#include "vk_core/utils/ResourceHandle.h"

namespace lcf::vkc {

class Sampler
{
    using Self = Sampler;
    using ResourceHandle = utils::ResourceHandle<vk::Sampler>;
public:
    ~Sampler() noexcept = default;
    Sampler() noexcept = default;
    Sampler(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Sampler(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator const vk::Sampler &() const noexcept { return this->handle(); }
public:
    std::error_code create(vk::Device device, const SamplerInfo & info) noexcept;
    const vk::Sampler & handle() const noexcept { return m_sampler_rh.get(); }
    ResourceLease lease() const noexcept { return m_sampler_rh.lease(); }
    const SamplerInfo & getInfo() const noexcept { return m_info; }
private:
    ResourceHandle m_sampler_rh;
    SamplerInfo m_info;
};

} // namespace lcf::vkc
