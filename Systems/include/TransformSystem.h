#pragma once

#include "Entity.h"
#include "Transform.h"
#include "signals.h"
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/container/flat_map.hpp>

namespace lcf {
    class HierarchicalTransform;
    
    class TransformSystem
    {
    public:
        using Set = entt::basic_sparse_set<entt::entity>;
        using FrequentLargeList = boost::container::vector<EntityHandle, boost::pool_allocator<EntityHandle>>;
        static constexpr uint16_t s_frequent_level_count = 4;
        using FrequentLevelMap = std::array<FrequentLargeList, s_frequent_level_count>;
        using UnfrequentLevelMap = boost::container::flat_map<uint16_t, FrequentLargeList>;
        TransformSystem(Registry * registry_p);
        ~TransformSystem() = default;
        void onTransformUpdate(const TransformUpdateSignalInfo & info);
        void onTransformHierarchyAttach(const TransformHierarchyAttachSignalInfo & info);
        void onTransformHierarchyDetach(const TransformHierarchyDetachSignalInfo & info);
        void update();
    private:
        void attach(EntityHandle parent, EntityHandle child);
        void detach(EntityHandle entity);
        void markDirty(EntityHandle entity, HierarchicalTransform & hierarchy);
        void updateDirtyEntity(EntityHandle entity);
        void updateRecursive(EntityHandle entity, const Matrix4x4 & parent_world_matrix);
    private:
        Registry * m_registry_p;
        Set m_dirty_entities;
        UnfrequentLevelMap m_unfrequent_level_map;
        FrequentLevelMap m_frequent_level_map;
    };

    class HierarchicalTransform
    {
        friend class TransformSystem;
    public:
        using ChildrenList = boost::container::vector<
            EntityHandle, 
            boost::pool_allocator<EntityHandle>
        >;
        EntityHandle getParent() const { return m_parent; }
        std::span<const EntityHandle> getChildren() const { return m_children; }
        uint32_t getLevel() const { return m_level; }
        bool isDirty() const { return m_dirty; }
        const Matrix4x4 & getWorldMatrix() const { return m_world_matrix; }
    private:
        void addChild(EntityHandle child) // O(1), 不重复由TransformSystem负责
        {
            m_children.emplace_back(child);
        }
        void removeChild(EntityHandle child) // O(n)
        {
            auto it = std::ranges::find(m_children, child);
            if (it == m_children.end()) { return; }
            std::swap(*it, m_children.back());
            m_children.pop_back();
        }
        void setParent(EntityHandle parent)  { m_parent = parent; }
        void setLevel(uint16_t level) { m_level = level; }
        void markDirty() { m_dirty = true; }
        void markClean() { m_dirty = false; }
        void setWorldMatrix(const Matrix4x4 & world_matrix) { m_world_matrix = world_matrix; }
    private:
        Matrix4x4 m_world_matrix;
        ChildrenList m_children;
        EntityHandle m_parent = entt::null;
        uint16_t m_level = 0;
        bool m_dirty = true;
    };
}