#pragma once

#include "RenderPrimitive.h"
#include "Matrix.h"
#include <vector>
#include "Registry.h"
#include "Entity.h"

namespace lcf {
    class RenderableModelLoader;

    class RenderableModel
    {
        friend class RenderableModelLoader;
        using Self = RenderableModel;
    public:
        struct HierarchyNode
        {
            HierarchyNode() = default;
            ~HierarchyNode() = default;
            HierarchyNode(const HierarchyNode &) = default;
            HierarchyNode & operator=(const HierarchyNode &) = default;
            HierarchyNode(HierarchyNode &&) = default;
            HierarchyNode & operator=(HierarchyNode &&) = default;
            size_t m_parent_index = -1u; // -1u means no parent
            std::vector<size_t> m_primitive_indices; // node's controlled primitives
            Matrix4x4 m_local_matrix;
        };
        using HierarchyNodeList = std::vector<HierarchyNode>;
    public:
        RenderableModel() = default;
        ~RenderableModel() = default;
        RenderableModel(const RenderableModel &) = delete;
        RenderableModel &operator=(const RenderableModel &) = delete;
        RenderableModel(RenderableModel &&) = default;
        RenderableModel &operator=(RenderableModel &&) = default;
    public:
        std::vector<Entity> generateEntities(Registry & registry) const noexcept;
        Entity generateEntity(Registry & registry) const noexcept;
    private:
        RenderPrimitiveList m_render_primitive_list;
        HierarchyNodeList m_hierarchy_node_list;
    };
}