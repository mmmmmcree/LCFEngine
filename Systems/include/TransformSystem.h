#pragma once

#include "Entity.h"
#include "Transform.h"
#include "signals.h"
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace lcf {
    class HierarchicalTransform;

    class TransformSystem
    {
    public:
        using Set = entt::basic_sparse_set<entt::entity>;
        using FrequentLargeList = std::vector<EntityHandle>;
        // using FrequentLargeList = boost::container::vector<EntityHandle, boost::pool_allocator<EntityHandle>>;
        static constexpr uint16_t s_frequent_level_count = 4;
        using FrequentLevelMap = std::array<FrequentLargeList, s_frequent_level_count>;
        using UnfrequentLevelMap = entt::dense_map<uint16_t, FrequentLargeList>;
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
        Set m_dirty_entities;
        UnfrequentLevelMap m_unfrequent_level_map;
        FrequentLevelMap m_frequent_level_map;
    public:
        inline static int s_update_count = 0;
    };

    class HierarchicalTransform
    {
        friend class TransformSystem;
    public:
        using ChildrenList = boost::container::vector<EntityHandle, boost::pool_allocator<EntityHandle>>;
        const ChildrenList & getChildren() const noexcept { return m_children; }
        EntityHandle getParent() const noexcept { return m_parent; }
        void markDirty() noexcept { m_dirty = true; }
        void markClean() noexcept { m_dirty = false; }
        bool isDirty() const noexcept { return m_dirty; }
        void setWorldMatrix(const Matrix4x4 & world_matrix) noexcept { m_world_matrix = world_matrix; }
        void setWorldMatrix(Matrix4x4 && world_matrix) noexcept { m_world_matrix = std::move(world_matrix); }
        const Matrix4x4 & getWorldMatrix() const noexcept { return m_world_matrix; }
        uint16_t getLevel() const noexcept { return m_level; }
    private:
        void setLevel(uint16_t level) noexcept { m_level = level; }
        void setParent(EntityHandle parent) noexcept { m_parent = parent; }
        void addChild(EntityHandle child) { m_children.emplace_back(child); }
    private:
        Matrix4x4 m_world_matrix;
        ChildrenList m_children;
        uint16_t m_level = 0;
        bool m_dirty = true;
        EntityHandle m_parent = null_entity;
    };
}