#pragma once

namespace lcf {
    class ResourceEntityLifecycle;

    class ResourceLease
    {
    public:
        ResourceLease() = default;
        ResourceLease(ResourceEntityLifecycle & entity_lifecycle);
        ~ResourceLease() noexcept;
        ResourceLease(const ResourceLease & other) noexcept;
        ResourceLease & operator=(const ResourceLease & other) noexcept;
        ResourceLease(ResourceLease && other) noexcept;
        ResourceLease & operator=(ResourceLease && other) noexcept;
    private:
        void tryDestroy() noexcept;
    private:
        ResourceEntityLifecycle * m_entity_lifecycle_p = nullptr;
    };
}