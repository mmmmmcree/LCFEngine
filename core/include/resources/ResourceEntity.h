#pragma once

#include "resource_utils.h"
#include "ResourceRegistry.h"
#include "ResourceID.h"
#include "resources_signals.h"

namespace lcf {
    template <typename Resource>
    class ResourceEntity
    {
    public:
        ResourceEntity() = default;
        ResourceEntity(ResourceRegistry & registry) :
            m_registry_p(&registry),
            m_artifact_id(registry.create())
        {
            m_control_block_p =new ResourceControlBlock([registry_p = m_registry_p, artifact_id = m_artifact_id]() {
                if (not registry_p or artifact_id == ecs::null) { return; }
                auto state = registry_p->get<ResourceState>(artifact_id);
                if (state == ResourceState::eLoaded) {
                    registry_p->triggerSignal(ResourceReleasedSignal<Resource>(artifact_id, registry_p->get<Resource>(artifact_id)));
                }
                registry_p->destroy(artifact_id);
            });
            m_control_block_p->increaseRefCount();
        }
        ~ResourceEntity() noexcept { this->tryDestroy(); }
        ResourceEntity(const ResourceEntity & other) noexcept :
            m_registry_p(other.m_registry_p),
            m_control_block_p(other.m_control_block_p),
            m_artifact_id(other.m_artifact_id)
        {
            m_control_block_p->increaseRefCount();
        }
        ResourceEntity & operator=(const ResourceEntity & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_registry_p = other.m_registry_p;
            m_control_block_p = other.m_control_block_p;
            m_artifact_id = other.m_artifact_id;
            m_control_block_p->increaseRefCount();
            return *this;
        }
        ResourceEntity(ResourceEntity && other) noexcept :
            m_registry_p(std::exchange(other.m_registry_p, nullptr)),
            m_control_block_p(std::exchange(other.m_control_block_p, nullptr)),
            m_artifact_id(std::exchange(other.m_artifact_id, ecs::null))
        {
        }
        ResourceEntity & operator=(ResourceEntity && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_registry_p = std::exchange(other.m_registry_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
            m_artifact_id = std::exchange(other.m_artifact_id, ecs::null);
            return *this;
        }
        operator bool() const noexcept { return this->isValid(); }
        Resource & operator*() const noexcept { return m_registry_p->get<Resource>(m_artifact_id); }
        Resource * operator->() const noexcept { return m_registry_p->try_get<Resource>(m_artifact_id); }
    public:
        bool isValid() const noexcept { return m_artifact_id != ecs::null and this->getState() == ResourceState::eLoaded; }
        const ResourceArtifactID & getArtifactID() const noexcept { return m_artifact_id; }
        const ResourceState & getState() const noexcept { return m_registry_p->get<ResourceState>(m_artifact_id); }
        ResourceLease lease() const noexcept { return ResourceLease(m_control_block_p); }
    
    private:
        void tryDestroy() noexcept
        {
            if (not m_control_block_p) { return; }
            auto * control_block_p = std::exchange(m_control_block_p, nullptr);
            if (control_block_p->decreaseRefCountAndShouldDestroy()) {
                delete control_block_p;
            }
            m_registry_p = nullptr;
            m_artifact_id = ecs::null;
        }
    private:
        ResourceRegistry * m_registry_p = nullptr;
        ResourceArtifactID m_artifact_id = ecs::null;
        ResourceControlBlock * m_control_block_p = nullptr;
        //uuid
    };
}
