#include "vk_core/pipeline/shader/ShaderObjects.h"
#include "vk_core/pipeline/shader/ShaderObject.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <ranges>

namespace stdr = std::ranges;

namespace {

template <typename BitType>
constexpr void for_each_flag_bit(vk::Flags<BitType> flags, auto && visitor) noexcept
{
    using MaskType = typename vk::Flags<BitType>::MaskType;
    auto bits = static_cast<MaskType>(flags);
    while (bits) {
        auto lowest = bits & ~(bits - 1);
        visitor(static_cast<BitType>(lowest));
        bits ^= lowest;
    }
}

} // anonymous namespace

namespace lcf::vkc {

auto ShaderObjects::setStage(const ShaderObject &shader) noexcept -> Self &
{
    uint32_t slot = this->findOrInsertSlot(shader.getStage());
    m_handles[slot] = shader.handle();
    m_leases[slot] = shader.lease();
    return *this;
}

auto ShaderObjects::unsetStages(vk::ShaderStageFlags stages) noexcept -> Self &
{
    for_each_flag_bit(stages, [this](vk::ShaderStageFlagBits stage) {
        uint32_t slot = this->findOrInsertSlot(stage);
        m_handles[slot] = nullptr;
        m_leases[slot] = {};
    });
    return *this;
}
auto ShaderObjects::removeStage(vk::ShaderStageFlags stages) noexcept -> Self &
{
    for_each_flag_bit(stages, [this](vk::ShaderStageFlagBits stage) {
        auto it = stdr::lower_bound(m_stages, stage);
        if (it == m_stages.end() or *it != stage) { return; }
        auto slot = std::distance(m_stages.begin(), it);
        m_stages.erase(it);
        m_handles.erase(m_handles.begin() + slot);
        m_leases.erase(m_leases.begin() + slot);
    });
    return *this;
}

void ShaderObjects::bind(CommandBufferProxy &cmd) const noexcept
{
    cmd.bindShadersEXT(m_stages, m_handles);
    cmd.pinLeases(m_leases);
}

uint32_t ShaderObjects::findOrInsertSlot(vk::ShaderStageFlagBits stage) noexcept
{
    auto it = stdr::lower_bound(m_stages, stage);
    auto slot = std::distance(m_stages.begin(), it);
    if (it != m_stages.end() and *it == stage) { return static_cast<uint32_t>(slot); }
    m_stages.insert(it, stage);
    m_handles.insert(m_handles.begin() + slot, nullptr);
    m_leases.emplace(m_leases.begin() + slot);
    return static_cast<uint32_t>(slot);
}

} // namespace lcf::vkc
