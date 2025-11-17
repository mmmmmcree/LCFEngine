#pragma once

#include "Entity.h"
#include "Transform.h"
#include "signals.h"
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace lcf {
    class TransformSystem
    {
    public:
        using Set = entt::basic_sparse_set<entt::entity>;
        // using HierarchicalToEntityMap = boost::container::flat_map<HierarchicalTransform *, EntityHandle>;
        // using HierarchicalToEntityMap = std::unordered_map<HierarchicalTransform *, EntityHandle>;
        using HierarchicalToEntityMap = boost::unordered_flat_map<HierarchicalTransform *, EntityHandle>;
        // using FrequentLargeList = boost::container::vector<EntityHandle, boost::pool_allocator<EntityHandle>>;
        using FrequentLargeList = std::vector<EntityHandle>;
        static constexpr uint16_t s_frequent_level_count = 4;
        using FrequentLevelMap = std::array<FrequentLargeList, s_frequent_level_count>;
        using UnfrequentLevelMap = boost::unordered_flat_map<uint16_t, FrequentLargeList>;
        TransformSystem(Registry & registry);
        ~TransformSystem();
        void onTransformUpdate(const TransformUpdateSignalInfo & info);
        void onTransformHierarchyAttach(const TransformHierarchyAttachSignalInfo & info);
        void onTransformHierarchyDetach(const TransformHierarchyDetachSignalInfo & info);
        void updateBFS();
        void updateDFS();
    private:
        void attach(EntityHandle parent, EntityHandle child);
        void detach(EntityHandle entity);
        void markDirty(EntityHandle entity, HierarchicalTransform & hierarchy);
        void updateDirtyEntityBFS(EntityHandle entity) noexcept;
        void updateDirtyEntityDFS(EntityHandle entity) noexcept;
        void updateRecursively(EntityHandle entity, const Matrix4x4 & parent_world_matrix) noexcept;
    private:
        Registry * m_registry_p;
        HierarchicalToEntityMap m_hierarchical_to_entity_map;
        Set m_dirty_entities;
        UnfrequentLevelMap m_unfrequent_level_map;
        FrequentLevelMap m_frequent_level_map;
    };
}