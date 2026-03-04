#pragma once

#include "Registry.h"
#include "ResourceID.h"

namespace lcf {
    class ResourceEntity
    {
    public:
        ResourceEntity(Registry & registry) : m_registry_p(&registry), m_artifact_id(registry.create()) {}
        ~ResourceEntity() noexcept { m_registry_p->destroy(m_artifact_id); } //todo if necessary, emit signal here with artifact_id, no specific Resource
        ResourceEntity(const ResourceEntity &) = delete;
        ResourceEntity & operator=(const ResourceEntity &) = delete;
        ResourceEntity(ResourceEntity &&) noexcept = default;
        ResourceEntity & operator=(ResourceEntity &&) noexcept = default;
        const ResourceArtifactID & getArtifactID() const noexcept { return m_artifact_id; }
        template <typename T>
        T & get() const noexcept { return m_registry_p->get<T>(m_artifact_id); }
        template <typename T>
        bool has() const noexcept { return m_registry_p->any_of<T>(m_artifact_id); }
    private:
        Registry * m_registry_p;
        ResourceArtifactID m_artifact_id;
    };

    class ResourceEntityLifecycle
    {
    public:
        ResourceEntityLifecycle(Registry & registry) :
            m_entity(registry),
            m_tenant_count(0),
            m_owner_count(1)
        {}
        ~ResourceEntityLifecycle() noexcept = default;
        ResourceEntityLifecycle(const ResourceEntityLifecycle &) = delete;
        ResourceEntityLifecycle & operator=(const ResourceEntityLifecycle &) = delete;
        ResourceEntityLifecycle(ResourceEntityLifecycle &&) noexcept = default;
        ResourceEntityLifecycle & operator=(ResourceEntityLifecycle &&) noexcept = default;
    public:
        ResourceEntity & getEntity() noexcept { return m_entity; }
        const ResourceEntity & getEntity() const noexcept { return m_entity; }
        uint32_t getTenantCount() const noexcept { return m_tenant_count.load(std::memory_order_relaxed); }
        uint32_t getOwnerCount() const noexcept { return m_owner_count.load(std::memory_order_relaxed); }
        bool shouldDestroy() const noexcept { return this->getTenantCount() + this->getOwnerCount() == 0; }
        uint32_t increaseTenantCount() noexcept { return m_tenant_count.fetch_add(1, std::memory_order_relaxed); }
        uint32_t decreaseTenantCount() noexcept { return m_tenant_count.fetch_sub(1, std::memory_order_relaxed); }
        uint32_t increaseOwnerCount() noexcept { return m_owner_count.fetch_add(1, std::memory_order_relaxed); }
        uint32_t decreaseOwnerCount() noexcept { return m_owner_count.fetch_sub(1, std::memory_order_relaxed); }
    private:
        ResourceEntity m_entity;
        std::atomic<uint32_t> m_tenant_count;
        std::atomic<uint32_t> m_owner_count;
    };
}