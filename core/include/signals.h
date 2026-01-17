#pragma once

#include "ecs_fwd_decls.h"

namespace lcf {
    struct EntitySignalBase
    {
        EntitySignalBase() = default;
        EntitySignalBase(EntityHandle entity) : m_sender(entity) {}
        EntityHandle m_sender = null_entity_handle;
    };

    template <typename SignalInfo>
    concept entity_eignal_info_c = std::derived_from<SignalInfo, EntitySignalBase>;

    struct TransformAttachSignal : EntitySignalBase
    {
        TransformAttachSignal(EntityHandle parent) : m_parent(parent) {}
        EntityHandle m_parent;
    };

    struct TransformUpdateSignal : EntitySignalBase { };

    struct TransformDetachSignal : EntitySignalBase { };
}