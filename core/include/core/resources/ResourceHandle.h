#pragma once

#include "ResourceID.h"
#include "resources_enums.h"
#include "resources_signals.h"
#include "Registry.h"

namespace lcf {
    template <typename Resource>
    class ResourceHandle;

    class ResourceEntity
    {
        template <typename> friend class ResourceHandle;
    public:
        ResourceEntity(Registry & registry) : m_registry_p(&registry), m_artifact_id(registry.create()) {}
        ~ResourceEntity() noexcept { m_registry_p->destroy(m_artifact_id); }
        ResourceEntity(const ResourceEntity &) = delete;
        ResourceEntity & operator=(const ResourceEntity &) = delete;
        ResourceEntity(ResourceEntity &&) noexcept = default;
        ResourceEntity & operator=(ResourceEntity &&) noexcept = default;
        template <typename T>
        T & get() const noexcept { return m_registry_p->get<T>(m_artifact_id); }
        template <typename T>
        bool has() const noexcept { return m_registry_p->any_of<T>(m_artifact_id); }
    private:
        Registry * m_registry_p;
        ResourceArtifactID m_artifact_id;
    };

    class ResourceLeash
    {
        template <typename> friend class ResourceHandle;
    public:
        ResourceLeash() = default;
        ResourceLeash(Registry & registry) : m_entity_sp(std::make_shared<ResourceEntity>(registry)) {}
        ResourceLeash(const ResourceLeash &) = default;
        ResourceLeash & operator=(const ResourceLeash &) = default;
        ResourceLeash(ResourceLeash &&) noexcept = default;
        ResourceLeash & operator=(ResourceLeash &&) noexcept = default;
        operator bool() const noexcept { return bool(m_entity_sp); }
        uint32_t getRefCount() const noexcept { return m_entity_sp.use_count(); }
    private:
        std::shared_ptr<ResourceEntity> m_entity_sp;
    };

    template <typename Resource>
    class ResourceHandle
    {
    public:
        ResourceHandle() = default;
        ResourceHandle(Registry & registry) : m_leash(registry) {}
        ~ResourceHandle() noexcept
        {
            if (m_leash.getRefCount() == 1) {
                //todo emit signal here
            }
        }
        ResourceHandle(const ResourceHandle &) = default;
        ResourceHandle & operator=(const ResourceHandle &) = default;
        ResourceHandle(ResourceHandle &&) noexcept = default;
        ResourceHandle & operator=(ResourceHandle &&) noexcept = default;
        operator bool() const noexcept { return this->isValid(); }
        Resource & operator*() const noexcept { return m_leash.m_entity_sp->get<Resource>(); }
        Resource * operator->() const noexcept { return &m_leash.m_entity_sp->get<Resource>(); }
        bool isValid() const noexcept { return bool(m_leash); }
        ResourceLeash getLeash() const noexcept { return m_leash; }
        ResourceArtifactID getArtifactID() const noexcept{ return m_leash.m_entity_sp->m_artifact_id; }
        ResourceState getState() const noexcept { return m_leash.m_entity_sp->get<ResourceState>(); }
    private:
        ResourceLeash m_leash;
    };
}