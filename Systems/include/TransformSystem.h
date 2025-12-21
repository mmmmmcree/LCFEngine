#pragma once

#include "Entity.h"
#include "Transform.h"
#include "signals.h"

namespace lcf {
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
        void attach(EntityHandle parent, EntityHandle child);
        void detach(EntityHandle entity);
        void markDirty(EntityHandle entity) noexcept;
    private:
        Registry * m_registry_p;
    };

    struct TransformHierarchy
    {
        using ChildrenList = std::vector<EntityHandle>;
        void setParent(EntityHandle parent) noexcept { m_parent = parent; }
        EntityHandle getParent() const noexcept { return m_parent; }
        void addChild(EntityHandle child) { m_children.emplace_back(child); }
        void removeChild(EntityHandle child);
        const ChildrenList & getChildren() const noexcept { return m_children; }
        EntityHandle m_parent;
        ChildrenList m_children;
    };
}