#pragma once

#include <system_error>
#include <utility>

namespace lcf::vkc {

class CommandBufferProxy;

}

namespace lcf::vkc::dsh {

class DescriptorHeap;

class DescriptorHeapAccess
{
public:
    ~DescriptorHeapAccess() noexcept = default;
    explicit DescriptorHeapAccess(DescriptorHeap & heap) noexcept : m_heap_p(&heap) {}
    DescriptorHeapAccess() noexcept = default;
    DescriptorHeapAccess(const DescriptorHeapAccess &) = delete;
    DescriptorHeapAccess & operator=(const DescriptorHeapAccess &) = delete;
    DescriptorHeapAccess(DescriptorHeapAccess && other) noexcept : m_heap_p(std::exchange(other.m_heap_p, nullptr)), m_resource_version(std::exchange(other.m_resource_version, 0u)), m_sampler_version(std::exchange(other.m_sampler_version, 0u)) {}
    DescriptorHeapAccess & operator=(DescriptorHeapAccess && other) noexcept { if (this != &other) { m_heap_p = std::exchange(other.m_heap_p, nullptr); m_resource_version = std::exchange(other.m_resource_version, 0u); m_sampler_version = std::exchange(other.m_sampler_version, 0u); } return *this; }
public:
    std::error_code updateIfDirty(CommandBufferProxy & cmd) noexcept;
    void bind(lcf::vkc::CommandBufferProxy & cmd) const noexcept;
private:
    DescriptorHeap * m_heap_p = nullptr;
    uint64_t m_resource_version = 0u;
    uint64_t m_sampler_version = 0u;
};

} // namespace lcf::vkc::dsh
