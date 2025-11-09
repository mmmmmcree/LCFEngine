#pragma once

#include "Entity.h"

namespace lcf {
    struct TransformHierarchyAttachSignalInfo : EntitySignalInfoBase
    {
        TransformHierarchyAttachSignalInfo(EntityHandle parent) : m_parent(parent) {}
        EntityHandle m_parent;
    };

    struct TransformUpdateSignalInfo : EntitySignalInfoBase { };

    struct TransformHierarchyDetachSignalInfo : EntitySignalInfoBase { };
}