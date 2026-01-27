#include "render_assets/RenderableModel.h"
#include "Transform.h"
#include "signals.h"
#include <ranges>

namespace stdv = std::views;
using namespace lcf;

std::vector<Entity> RenderableModel::generateEntities(Registry &registry) const noexcept
{
    if (m_hierarchy_node_list.empty()) { return {}; }
    std::vector<Entity> entities(m_hierarchy_node_list.size());
    for (auto & entity : entities) { entity.create(registry); }
    for (const auto & [i, node] : stdv::enumerate(m_hierarchy_node_list) | stdv::drop(1) | stdv::reverse) {
        const auto & node = m_hierarchy_node_list[i];
        auto & entity = entities[i];
        auto & parent_entity = entities[node.m_parent_index];
        auto & transform = entity.requireComponent<Transform>();
        transform.setLocalMatrix(node.m_local_matrix);
        entity.enqueueSignal<TransformAttachSignal>({parent_entity.getHandle()});
        auto & render_primitive_list = parent_entity.requireComponent<RenderPrimitiveList>();
        render_primitive_list.assign_range(node.m_primitive_indices | stdv::transform([&](const auto & index) {
            return m_render_primitive_list[index];
        }));
    }
    const auto & root_node = m_hierarchy_node_list.front();
    auto & main_entity = entities.front();
    auto & transform = main_entity.requireComponent<Transform>();
    transform.setLocalMatrix(root_node.m_local_matrix);
    // todo main entity will require Animation, Skeleton, etc. components
    return entities;
}

Entity RenderableModel::generateEntity(Registry &registry) const noexcept
{
    Entity entity {registry};
    auto & transform = entity.requireComponent<Transform>();
    if (not m_hierarchy_node_list.empty()) {
        const auto & root_node = m_hierarchy_node_list.front();
        transform.setLocalMatrix(root_node.m_local_matrix);
    }
    auto & render_primitive_list = entity.requireComponent<RenderPrimitiveList>();
    render_primitive_list.assign_range(m_render_primitive_list);
    return entity;
}
