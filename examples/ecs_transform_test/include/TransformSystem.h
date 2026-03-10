#pragma once

#include "ecs/Entity.h"
#include "ecs/signals.h"
#include "Vector.h"
#include "Matrix.h"
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <bitset>
#include <vector>

namespace lcf::ecs {
    struct TransformHierarchy;

    struct TransformState;

    class TransformSystem
    {
    public:
        using Set = entt::basic_sparse_set<entt::entity>;
        using FrequentLargeList = std::vector<EntityId>;
        static constexpr uint32_t s_frequent_level_count = 4;
        using FrequentLevelMap = std::array<FrequentLargeList, s_frequent_level_count>;
        using UnfrequentLevelMap = entt::dense_map<uint32_t, FrequentLargeList>;
        TransformSystem(Registry & registry);
        ~TransformSystem();
        void onTransformUpdate(const TransformUpdateSignal & info);
        void onTransformHierarchyAttach(const TransformAttachSignal & info);
        void onTransformHierarchyDetach(const TransformDetachSignal & info);
        void update() noexcept;
    private:
        void attach(EntityId parent, EntityId child);
        void detach(EntityId entity);
        void markDirty(EntityId entity) noexcept;
        void updateBFS(EntityId entity) noexcept;
        void updateDFS(EntityId entity) noexcept;
        void updateRecursively(EntityId entity) noexcept;
    private:
        Registry * m_registry_p;
        Set m_dirty_entities;
        UnfrequentLevelMap m_unfrequent_level_map;
        FrequentLevelMap m_frequent_level_map;
    };

    struct TransformHierarchy // 32 bytes
    {
        using ChildrenList = boost::container::vector<EntityId, boost::pool_allocator<EntityId>>;
        TransformHierarchy() = default;
        void setParent(EntityId parent) noexcept { m_parent = parent; }
        EntityId getParent() const noexcept { return m_parent; }
        void addChild(EntityId child) noexcept { m_children.emplace_back(child); }
        const ChildrenList & getChildren() const noexcept { return m_children; }
        void setLevel(uint32_t level) noexcept { m_level = level; }
        uint32_t getLevel() const noexcept { return m_level; }

        EntityId m_parent = entt::null;
        uint32_t m_level = 0;
        ChildrenList m_children;
    };

    // struct TransformState // 2 bytes
    // {
    //     TransformState() = default;
    //     void markDirty() noexcept { m_world_dirty = m_inverted_world_dirty = true; }
    //     void clean() noexcept { m_world_dirty = m_inverted_world_dirty = false; }
    //     void cleanWorld() noexcept { m_world_dirty = false; }
    //     void cleanInvertedWorld() noexcept { m_inverted_world_dirty = false; }
    //     bool isDirty() const noexcept { return m_world_dirty or m_inverted_world_dirty; }
    //     bool isWorldDirty() const noexcept { m_world_dirty; }
    //     bool isInvertedWorldDirty() const noexcept { return m_inverted_world_dirty; }
    // private:
    //     bool m_world_dirty = false;
    //     bool m_inverted_world_dirty = false;
    // };

    struct TransformState // 1 byte
    {
        TransformState() = default;
        void markDirty() noexcept { m_dirty = true; }
        void clean() noexcept { m_dirty = false; }
        bool isDirty() const noexcept { return m_dirty; }
    private:
        bool m_dirty = false;
    };

    class ENTTTransform
    {
    public:
        ENTTTransform() = default;
        void setWorldMatrix(const Matrix4x4 &matrix) noexcept { m_world_matrix = matrix; }
        const Matrix4x4 &getWorldMatrix() const noexcept { return m_world_matrix; }
        const Matrix4x4 &getLocalMatrix() const noexcept { return m_local_matrix; }
        void translateLocal(const Vector3D<float> &translation) noexcept { m_local_matrix.translateLocal(translation); }
    private:
        Matrix4x4 m_local_matrix;
        Matrix4x4 m_world_matrix;
    };
}