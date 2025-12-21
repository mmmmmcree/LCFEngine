#pragma once

#include "Entity.h"

namespace lcf {
    struct TransformAttachSignal : EntitySignalBase
    {
        TransformAttachSignal(EntityHandle parent) : m_parent(parent) {}
        EntityHandle m_parent;
    };

    struct TransformUpdateSignal : EntitySignalBase { };

    struct TransformHierarchyDetachSignalInfo : EntitySignalBase { };
}