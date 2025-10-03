#pragma once

#include "StandardSignal.h"
#include "Entity.h"

namespace lcf {
    struct TransformHierarchyAttachSignalInfo : EntitySignalInfoBase
    {
        EntityHandle parent;
    };

    struct TransformUpdateSignalInfo : EntitySignalInfoBase { };

    using TransformUpdateSignal = StandardSignal<TransformUpdateSignalInfo>;
    using TransformHierarchyAttachSignal = StandardSignal<TransformHierarchyAttachSignalInfo>;

    struct TransformHierarchyDetachSignalInfo : EntitySignalInfoBase { };
    using TransformHierarchyDetachSignal = StandardSignal<TransformHierarchyDetachSignalInfo>;
}