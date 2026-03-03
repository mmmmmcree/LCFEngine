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
}