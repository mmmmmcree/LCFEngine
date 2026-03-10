#pragma once

#include "resources_enums.h"
#include "ResourceID.h"

namespace lcf {
    struct ResourceStateChangedSignal
    {
        ResourceStateChangedSignal(ResourceArtifactID artifact_id, ResourceState state) :
            m_artifact_id(artifact_id), state(state) {}
        ResourceArtifactID m_artifact_id;
        ResourceState state;
    };

    template <typename Resource>
    struct ResourceReleasedSignal
    {
        ResourceReleasedSignal(ResourceArtifactID artifact_id, const Resource & resource) :
            m_artifact_id(artifact_id), m_resource(&resource) {}
        ResourceReleasedSignal(const ResourceReleasedSignal<Resource> & other) :
            m_artifact_id(other.m_artifact_id), m_resource(other.m_resource) {}
        ResourceReleasedSignal<Resource> & operator=(const ResourceReleasedSignal<Resource> & other)
        {
            if (this == &other) { return *this; }
            m_artifact_id = other.m_artifact_id;
            m_resource = other.m_resource;
            return *this;
        }
        ResourceReleasedSignal(ResourceReleasedSignal<Resource> && other) :
            m_artifact_id(std::exchange(other.m_artifact_id, ecs::null)), m_resource(std::exchange(other.m_resource, nullptr)) {}
        ResourceReleasedSignal<Resource> & operator=(ResourceReleasedSignal<Resource> && other)
        {
            if (this == &other) { return *this; }
            m_artifact_id = std::exchange(other.m_artifact_id, ecs::null);
            m_resource = std::exchange(other.m_resource, nullptr);
            return *this;
        }
        ResourceArtifactID getArtifactID() const { return m_artifact_id; }
        const Resource & getResource() const { return *m_resource; }

        ResourceArtifactID m_artifact_id;
        const Resource * m_resource;
    };

    struct ResourceDestroyedSignal
    {
        ResourceDestroyedSignal(ResourceArtifactID artifact_id) :
            m_artifact_id(artifact_id) {}
        ResourceArtifactID getArtifactID() const { return m_artifact_id; }

        ResourceArtifactID m_artifact_id;
    };
}