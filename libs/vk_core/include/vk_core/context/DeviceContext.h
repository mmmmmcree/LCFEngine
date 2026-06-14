#pragma once

#include "vk_core/context/QueueContext.h"
#include "vk_core/context/enums.h"
#include "enums/enum_count.h"
#include <array>
#include <span>
#include <utility>
#include <vector>

namespace lcf::vkc::conf {
    struct FeatureDependency;
} // namespace lcf::vkc::conf

namespace lcf::vkc {

class DeviceContextCreateInfo;

class DeviceContext
{
    using Self = DeviceContext;
public:
    DeviceContext() = default;
    ~DeviceContext() noexcept = default;
    DeviceContext(const Self &) = delete;
    DeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    std::error_code create(vk::Instance instance, const DeviceContextCreateInfo & create_info) noexcept;
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device; }
    std::span<QueueContext> queueContexts() noexcept { return m_queue_contexts; }
    QueueContext & getQueueContext(enums::QueueRole role) noexcept
    {
        return *m_mapped_queue_contexts[std::to_underlying(role)];
    }

private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    std::vector<QueueContext> m_queue_contexts;
    std::array<QueueContext *, enum_count_v<enums::QueueRole>> m_mapped_queue_contexts = {};
};

class DeviceContextCreateInfo
{
    using Self = DeviceContextCreateInfo;
    using FeatureDependencyList = std::vector<const conf::FeatureDependency *>;
    using Scorer = std::function<int(vk::PhysicalDevice)>;
public:
    Self & setPreferredDeviceType(vk::PhysicalDeviceType type) noexcept { m_preferred_type = type; return *this; }
    Self & setScorer(Scorer scorer) noexcept { m_scorer = std::move(scorer); return *this; }
    Self & addFeatureDependency(const conf::FeatureDependency & featrue_dependency)
    {
        m_feature_dependencies.emplace_back(&featrue_dependency);
        return *this;
    }

private:
    std::optional<vk::PhysicalDeviceType> m_preferred_type;
    Scorer m_scorer;
    FeatureDependencyList m_feature_dependencies;
};

} // namespace lcf::vkc
