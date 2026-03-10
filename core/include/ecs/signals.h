#pragma once

#include "ecs_fwd_decls.h"

namespace lcf::ecs {
    struct EntitySignalBase
    {
        EntitySignalBase(EntityId entity) : m_sender(entity) {}
        EntityId m_sender = null;
    };

    struct TransformAttachSignal : EntitySignalBase
    {
        TransformAttachSignal(EntityId sender, EntityId parent) :
            EntitySignalBase(sender),
            m_parent(parent) {}
        EntityId m_parent = null;
    };

    struct TransformUpdateSignal : EntitySignalBase { };

    struct TransformDetachSignal : EntitySignalBase { };
}