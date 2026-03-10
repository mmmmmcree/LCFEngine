#pragma once

#include "ecs/ecs_fwd_decls.h"
#include "ecs/signals.h"
#include "Transform.h"
#include <vector>

namespace lcf::ecs {
    struct TransformHierarchy;

    class TransformSystem
    {
    public:
        TransformSystem(Registry & registry);
        ~TransformSystem();
        void onTransformUpdate(const TransformUpdateSignal & info) noexcept;
        void onTransformHierarchyAttach(const TransformAttachSignal & info);
        void onTransformHierarchyDetach(const TransformDetachSignal & info);
        void update() noexcept;
    private:
        void attach(EntityId parent, EntityId child);
        void detach(EntityId entity);
        void markDirty(EntityId entity) noexcept;
    private:
        Registry * m_registry_p;
    };

    struct TransformHierarchy
    {
        using ChildrenList = std::vector<EntityId>;
        void setParent(EntityId parent) noexcept { m_parent = parent; }
        EntityId getParent() const noexcept { return m_parent; }
        void addChild(EntityId child) { m_children.emplace_back(child); }
        void removeChild(EntityId child);
        const ChildrenList & getChildren() const noexcept { return m_children; }
        EntityId m_parent;
        ChildrenList m_children;
    };
}