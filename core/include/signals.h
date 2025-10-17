#pragma once

#include "Entity.h"

namespace lcf {
    struct TransformHierarchyAttachSignalInfo : EntitySignalInfoBase
    {
        EntityHandle parent;
    };

    struct TransformUpdateSignalInfo : EntitySignalInfoBase { };

    struct TransformHierarchyDetachSignalInfo : EntitySignalInfoBase { };
}