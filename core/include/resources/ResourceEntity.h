#pragma once

#include "resource_utils.h"
#include "ResourceRegistry.h"
#include "ResourceID.h"
#include "resources_signals.h"

namespace lcf {

    class ResourceEntity
    {
        template <typename T> friend class TypedResourceEntity;
    public:
        ResourceEntity() = default;
        ResourceEntity(ResourceRegistry & registry) :
            m_registry_p(&registry),
            m_artifact_id(registry.create())
        {
            m_control_block_p = new ResourceControlBlock([registry_p = m_registry_p, artifact_id = m_artifact_id]() {
                if (not registry_p or artifact_id == ecs::null) { return; }
                registry_p->triggerSignal(ResourceReleasedSignal{artifact_id});
                registry_p->destroy(artifact_id);
            });
            m_control_block_p->increaseRefCount();
            m_control_block_p->increaseWeakRefCount(); // one collective weak ref for the strong side
        }
        ~ResourceEntity() noexcept { this->tryDestroy(); }
        ResourceEntity(const ResourceEntity & other) noexcept { this->copyFrom(other); }
        ResourceEntity & operator=(const ResourceEntity & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->copyFrom(other);
            return *this;
        }
        ResourceEntity(ResourceEntity && other) noexcept { this->stealFrom(other); }
        ResourceEntity & operator=(ResourceEntity && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->stealFrom(other);
            return *this;
        }
        operator bool() const noexcept { return this->isValid(); }
    private:
        ResourceEntity(ResourceRegistry * registry_p, ResourceArtifactID artifact_id, ResourceControlBlock * control_block_p) noexcept :
            m_registry_p(registry_p),
            m_artifact_id(artifact_id),
            m_control_block_p(control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
    public:
        bool isValid() const noexcept { return m_artifact_id != ecs::null; }
        ResourceArtifactID getArtifactID() const noexcept { return m_artifact_id; }
        ResourceLease lease() const noexcept { return ResourceLease(m_control_block_p); }
        template <typename T>
        T & get() const { return m_registry_p->get<T>(m_artifact_id); }
        template <typename T>
        T * tryGet() const { return m_registry_p->try_get<T>(m_artifact_id); }
        template <typename T> 
        bool has() const { return m_registry_p->any_of<T>(m_artifact_id); }
        template <typename T, typename... Args>
        T & emplace(Args &&... args) const { return m_registry_p->emplace<T>(m_artifact_id, std::forward<Args>(args)...); }
        template <typename T>
        void remove() const { m_registry_p->remove<T>(m_artifact_id); }
    private:
        void tryDestroy() noexcept
        {
            if (not m_control_block_p) { return; }
            auto * control_block_p = std::exchange(m_control_block_p, nullptr);
            if (control_block_p->decreaseRefCountAndShouldDestroy()) {
                control_block_p->destroyResource();
                if (control_block_p->decreaseWeakRefCountAndShouldDelete()) {
                    delete control_block_p;
                }
            }
            m_registry_p = nullptr;
            m_artifact_id = ecs::null;
        }
        void copyFrom(const ResourceEntity & other) noexcept
        {
            m_registry_p = other.m_registry_p;
            m_control_block_p = other.m_control_block_p;
            m_artifact_id = other.m_artifact_id;
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        void stealFrom(ResourceEntity & other) noexcept
        {
            m_registry_p = std::exchange(other.m_registry_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
            m_artifact_id = std::exchange(other.m_artifact_id, ecs::null);
        }
    private:
        ResourceRegistry * m_registry_p = nullptr;
        ResourceArtifactID m_artifact_id = ecs::null;
        ResourceControlBlock * m_control_block_p = nullptr;
    };

    template <typename T>
    class TypedResourceEntity
    {
    public:
        TypedResourceEntity() = default;
        TypedResourceEntity(const ResourceEntity & entity) noexcept :
            m_registry_p(entity.m_registry_p),
            m_artifact_id(entity.m_artifact_id),
            m_control_block_p(entity.m_control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        ~TypedResourceEntity() noexcept { this->tryDestroy(); }
        TypedResourceEntity(const TypedResourceEntity & other) noexcept { this->copyFrom(other); }
        TypedResourceEntity & operator=(const TypedResourceEntity & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->copyFrom(other);
            return *this;
        }
        TypedResourceEntity(TypedResourceEntity && other) noexcept { this->stealFrom(other); }
        TypedResourceEntity & operator=(TypedResourceEntity && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->stealFrom(other);
            return *this;
        }
        operator bool() const noexcept { return m_artifact_id != ecs::null; }
        operator ResourceEntity() const noexcept { return ResourceEntity(m_registry_p, m_artifact_id, m_control_block_p); }
        T & operator*() const noexcept { return m_registry_p->get<T>(m_artifact_id); }
        T * operator->() const noexcept { return m_registry_p->try_get<T>(m_artifact_id); }
    public:
        ResourceArtifactID getArtifactID() const noexcept { return m_artifact_id; }
        ResourceLease lease() const noexcept { return ResourceLease(m_control_block_p); }
    private:
        void tryDestroy() noexcept
        {
            if (not m_control_block_p) { return; }
            auto * control_block_p = std::exchange(m_control_block_p, nullptr);
            if (control_block_p->decreaseRefCountAndShouldDestroy()) {
                control_block_p->destroyResource();
                if (control_block_p->decreaseWeakRefCountAndShouldDelete()) {
                    delete control_block_p;
                }
            }
            m_registry_p = nullptr;
            m_artifact_id = ecs::null;
        }
        void copyFrom(const TypedResourceEntity & other) noexcept
        {
            m_registry_p = other.m_registry_p;
            m_control_block_p = other.m_control_block_p;
            m_artifact_id = other.m_artifact_id;
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        void stealFrom(TypedResourceEntity & other) noexcept
        {
            m_registry_p = std::exchange(other.m_registry_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
            m_artifact_id = std::exchange(other.m_artifact_id, ecs::null);
        }
    private:
        ResourceRegistry * m_registry_p = nullptr;
        ResourceArtifactID m_artifact_id = ecs::null;
        ResourceControlBlock * m_control_block_p = nullptr;
    };
}
