#pragma once

#include "Entity.h"
#include "Transform.h"
#include "signals.h"
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <bitset>

namespace lcf {
    struct TransformHierarchy;

    class TransformSystem
    {
    public:
        using Set = entt::basic_sparse_set<entt::entity>;
        using FrequentLargeList = std::vector<EntityHandle>;
        static constexpr uint32_t s_frequent_level_count = 4;
        using FrequentLevelMap = std::array<FrequentLargeList, s_frequent_level_count>;
        using UnfrequentLevelMap = entt::dense_map<uint32_t, FrequentLargeList>;
        TransformSystem(Registry & registry);
        ~TransformSystem();
        void onTransformUpdate(const TransformUpdateSignalInfo & info) noexcept;
        void onTransformHierarchyAttach(const TransformHierarchyAttachSignalInfo & info);
        void onTransformHierarchyDetach(const TransformHierarchyDetachSignalInfo & info);
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