#pragma once

#include <vulkan/vulkan.hpp>
#include <type_traits>
#include <utility>
#include "resource_utils.h"

namespace lcf::vkc::utils {

namespace details {

template <typename T>
concept vk_handle_c = vk::isVulkanHandleType<T>::value and std::is_trivially_destructible_v<T>;

template <typename Deleter>
struct deleter_access : Deleter
{
    using Deleter::destroy;
};

} // namespace lcf::vkc::utils::details

template <details::vk_handle_c Handle>
class ResourceHandle
{
    using Self = ResourceHandle<Handle>;
public:
    ~ResourceHandle() noexcept { this->tryDestroy(); }
    ResourceHandle() noexcept = default;
    template <typename Dispatch>
    ResourceHandle(vk::UniqueHandle<Handle, Dispatch> && unique_handle) noexcept
    {
        if (not unique_handle) { return; }
        using Deleter = typename vk::UniqueHandleTraits<Handle, Dispatch>::deleter;
        details::deleter_access<Deleter> deleter { static_cast<const Deleter &>(unique_handle) };
        m_handle = unique_handle.get();
        m_control_block_p = new ResourceControlBlock([deleter, handle = m_handle]() mutable { deleter.destroy(handle); });
        m_control_block_p->increaseRefCount();
        m_control_block_p->increaseWeakRefCount();
        (void)unique_handle.release();
    }
    explicit ResourceHandle(Handle handle, ResourceControlBlock::deleter_t deleter = nullptr) noexcept
    {
        if (not handle) { return; }
        m_handle = handle;
        m_control_block_p = new ResourceControlBlock(std::move(deleter));
        m_control_block_p->increaseRefCount();
        m_control_block_p->increaseWeakRefCount();
    };
    ResourceHandle(const Self & other) noexcept { this->copyFrom(other); }
    Self & operator=(const Self & other) noexcept
    {
        if (this == &other) { return *this; }
        this->tryDestroy();
        this->copyFrom(other);
        return *this;
    }
    ResourceHandle(Self && other) noexcept { this->stealFrom(other); }
    Self & operator=(Self && other) noexcept
    {
        if (this == &other) { return *this; }
        this->tryDestroy();
        this->stealFrom(other);
        return *this;
    }
    template <typename Dispatch>
    Self & operator=(vk::UniqueHandle<Handle, Dispatch> && unique_handle)
    {
        return this->operator=(Self(std::move(unique_handle)));
    }
    operator const Handle &() const noexcept { return m_handle; }
    explicit operator bool() const noexcept { return m_control_block_p; }
public:
    const Handle & get() const noexcept { return m_handle; }
    const Handle * operator->() const noexcept { return &m_handle; }
    ResourceLease lease() const noexcept { return ResourceLease(m_control_block_p); }
private:
    void copyFrom(const Self & other) noexcept
    {
        m_handle = other.m_handle;
        m_control_block_p = other.m_control_block_p;
        if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
    }
    void stealFrom(Self & other) noexcept
    {
        m_handle = std::exchange(other.m_handle, Handle {});
        m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
    }
    void tryDestroy() noexcept
    {
        m_handle = Handle {};
        if (not m_control_block_p) { return; }
        if (m_control_block_p->decreaseRefCountAndShouldDestroy()) {
            m_control_block_p->destroyResource();
            if (m_control_block_p->decreaseWeakRefCountAndShouldDelete()) {
                delete m_control_block_p;
            }
        }
        m_control_block_p = nullptr;
    }
private:
    Handle m_handle = {};
    ResourceControlBlock * m_control_block_p = nullptr;
};

} // namespace lcf::vkc::utils
