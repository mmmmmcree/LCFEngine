#include "vk_core/sampler/Sampler.h"

namespace lcf::vkc {

std::error_code Sampler::create(vk::Device device, const SamplerInfo & info) noexcept
{
    try {
        m_sampler_rh = device.createSamplerUnique(info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    m_info = info;
    return {};
}

} // namespace lcf::vkc
