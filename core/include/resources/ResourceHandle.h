#pragma once

#include "ResourceEntity.h"
#include "ResourceLease.h"
#include "resources_enums.h"
#include "resources_signals.h"

namespace lcf {
    template <typename Resource>
    class ResourceHandle
    {
    public:
        ResourceHandle() = default;
        ResourceHandle(ResourceRegistry & registry) : m_entity_lifecycle_p(new ResourceEntityLifecycle(registry)) {}
        ~ResourceHandle() noexcept { this->tryDestroy(); }
        ResourceHandle(const ResourceHandle & other) noexcept : m_entity_lifecycle_p(other.m_entity_lifecycle_p)
        {
            if (m_entity_lifecycle_p) { m_entity_lifecycle_p->increaseOwnerCount(); }
        }
        ResourceHandle & operator=(const ResourceHandle & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_entity_lifecycle_p = other.m_entity_lifecycle_p;
            if (m_entity_lifecycle_p) { m_entity_lifecycle_p->increaseOwnerCount(); }
            return *this;
        }
        ResourceHandle(ResourceHandle && other) noexcept : m_entity_lifecycle_p(std::exchange(other.m_entity_lifecycle_p, nullptr)) {}
        ResourceHandle & operator=(ResourceHandle && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_entity_lifecycle_p = std::exchange(other.m_entity_lifecycle_p, nullptr);
            return *this;
        }
        operator bool() const noexcept { return this->isValid(); }
        Resource & operator*() const noexcept { return m_entity_lifecycle_p->getEntity().get<Resource>(); }
        Resource * operator->() const noexcept { return &m_entity_lifecycle_p->getEntity().get<Resource>(); }
        bool isValid() const noexcept { return m_entity_lifecycle_p and m_entity_lifecycle_p->getEntity().has<Resource>(); }
        ResourceLease getLease() const noexcept { return ResourceLease(*m_entity_lifecycle_p); }
        ResourceArtifactID getArtifactID() const noexcept{ return m_entity_lifecycle_p->getEntity().getArtifactID(); }
        ResourceState getState() const noexcept { return m_entity_lifecycle_p->getEntity().get<ResourceState>(); }
    private:
        void tryDestroy() noexcept
        {
            if (not m_entity_lifecycle_p) { return; }
            if (m_entity_lifecycle_p->decreaseOwnerCount() == 1)  {
                auto & entity = m_entity_lifecycle_p->getEntity();
                if (entity.has<Resource>()) {
                    entity.triggerSignal<ResourceReleasedSignal<Resource>>({entity.getArtifactID(), entity.get<Resource>()});
                }
            }
            if (m_entity_lifecycle_p->shouldDestroy()) {
                delete m_entity_lifecycle_p;
                m_entity_lifecycle_p = nullptr;
            }
        }
    private:
        ResourceEntityLifecycle * m_entity_lifecycle_p = nullptr;
    };
}