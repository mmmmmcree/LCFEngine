#include "TransformSystem.h"
#include "signals.h"
#include <algorithm>

lcf::TransformSystem::TransformSystem(Registry & registry) :
    m_registry_p(&registry)
{
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    dispatcher.sink<lcf::TransformHierarchyAttachSignalInfo>().connect<&lcf::TransformSystem::onTransformHierarchyAttach>(*this);
    dispatcher.sink<lcf::TransformHierarchyDetachSignalInfo>().connect<&lcf::TransformSystem::onTransformHierarchyDetach>(*this);
    dispatcher.sink<lcf::TransformUpdateSignalInfo>().connect<&lcf::TransformSystem::onTransformUpdate>(*this);
}

lcf::TransformSystem::~TransformSystem()
{
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    dispatcher.sink<lcf::TransformHierarchyAttachSignalInfo>().disconnect(this);
    dispatcher.sink<lcf::TransformHierarchyDetachSignalInfo>().disconnect(this);
    dispatcher.sink<lcf::TransformUpdateSignalInfo>().disconnect(this);
}

void lcf::TransformSystem::onTransformUpdate(const TransformUpdateSignalInfo &info)
{
    EntityHandle entity = info.m_sender;
    HierarchicalTransform * hierarchy = m_registry_p->try_get<HierarchicalTransform>(entity);
    if (not hierarchy) { return; }
    this->markDirty(entity, *hierarchy);
}

void lcf::TransformSystem::onTransformHierarchyAttach(const TransformHierarchyAttachSignalInfo &info)
{
    this->attach(info.m_parent, info.m_sender);    
}

void lcf::TransformSystem::onTransformHierarchyDetach(const TransformHierarchyDetachSignalInfo &info)
{
    this->detach(info.m_sender);
}

void lcf::TransformSystem::updateBFS()
{
    for (uint16_t level = 0; level < s_frequent_level_count; ++level) {
        auto & dirty_entitys = m_frequent_level_map[level];
        for (auto entity : dirty_entitys) {
            this->updateDirtyEntityBFS(entity);
        }
        dirty_entitys.clear();
    }
    for (auto [level, entities] : m_unfrequent_level_map) {
        for (auto entity : entities) {
            this->updateDirtyEntityBFS(entity);
        }
    }
    m_unfrequent_level_map.clear();
    m_dirty_entities.clear();
}

void lcf::TransformSystem::updateDFS()
{
    for (uint16_t level = 0; level < s_frequent_level_count; ++level) {
        auto & dirty_entitys = m_frequent_level_map[level];
        for (auto entity : dirty_entitys) {
            this->updateDirtyEntityDFS(entity);
        }
        dirty_entitys.clear();
    }
    for (auto [level, entities] : m_unfrequent_level_map) {
        for (auto entity : entities) {
            this->updateDirtyEntityDFS(entity);
        }
    }
    m_unfrequent_level_map.clear();
    m_dirty_entities.clear();
}

void lcf::TransformSystem::attach(EntityHandle parent, EntityHandle child)
{
    if (child == parent or child == entt::null) { return; }
    auto & child_hierarchy = m_registry_p->get_or_emplace<lcf::HierarchicalTransform>(child);
    auto & child_transform = m_registry_p->get_or_emplace<lcf::Transform>(child);
    child_hierarchy.setWorldMatrix(child_transform.getWorldMatrix());
    if (parent == entt::null) { return; }
    auto & parent_hierarchy = m_registry_p->get_or_emplace<lcf::HierarchicalTransform>(parent);
    auto & parent_transform = m_registry_p->get_or_emplace<lcf::Transform>(parent);
    parent_hierarchy.setWorldMatrix(parent_transform.getWorldMatrix());
    child_hierarchy.setParent(parent);
    parent_hierarchy.addChild(child);
    child_hierarchy.setLevel(parent_hierarchy.getLevel() + 1);
    this->markDirty(child, child_hierarchy);
}

void lcf::TransformSystem::detach(EntityHandle entity)
{
    auto & transform = m_registry_p->get<Transform>(entity);
    transform.setWorldMatrix(&transform.getLocalMatrix());
    auto & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    hierarchy.setParent(null_entity);
    auto & parent_hierarchy = m_registry_p->get<HierarchicalTransform>(hierarchy.getParent());
    auto to_remove_it = std::ranges::find(parent_hierarchy.m_children, entity);
    if (to_remove_it != parent_hierarchy.m_children.end()) {
        std::swap(*to_remove_it, parent_hierarchy.m_children.back());
        parent_hierarchy.m_children.pop_back();
    }
}

void lcf::TransformSystem::markDirty(EntityHandle entity, HierarchicalTransform &hierarchy)
{
    if (m_dirty_entities.contains(entity)) { return; }
    m_dirty_entities.push(entity);
    uint16_t level = hierarchy.getLevel();
    if (level < s_frequent_level_count) {
        m_frequent_level_map[level].emplace_back(entity);
    } else {
        m_unfrequent_level_map[level].emplace_back(entity);
    }
    hierarchy.markDirty();
}

void lcf::TransformSystem::updateDirtyEntityBFS(EntityHandle entity) noexcept
{
    HierarchicalTransform & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    if (not hierarchy.isDirty()) { return; } //- the return condition shows that the entity is updated by their parent
    auto & transform = m_registry_p->get<Transform>(entity);
    EntityHandle parent = hierarchy.getParent();
    if (parent == entt::null) {
        hierarchy.setWorldMatrix(transform.getLocalMatrix());
    } else {
        Transform & parent_transform = m_registry_p->get<Transform>(parent);
        hierarchy.setWorldMatrix(parent_transform.getWorldMatrix() * transform.getLocalMatrix());
    }
    std::deque<EntityHandle> bfs_queue;
    bfs_queue.emplace_back(entity);
    while (not bfs_queue.empty()) {
        auto cur_entity = bfs_queue.front();
        bfs_queue.pop_front();
        auto & cur_hierarchy = m_registry_p->get<HierarchicalTransform>(cur_entity);
        for (auto child : cur_hierarchy.getChildren()) {
            auto & child_hierarchy = m_registry_p->get<HierarchicalTransform>(child);
            auto & child_transform = m_registry_p->get<Transform>(child);
            child_hierarchy.setWorldMatrix(cur_hierarchy.getWorldMatrix() * child_transform.getLocalMatrix());
            child_hierarchy.markClean();
            bfs_queue.emplace_back(child);
        }
    }
    hierarchy.markClean();
}

void lcf::TransformSystem::updateDirtyEntityDFS(EntityHandle entity) noexcept
{
    HierarchicalTransform & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    if (not hierarchy.isDirty()) { return; } //- the return condition shows that the entity is updated by their parent
    Transform & transform = m_registry_p->get<Transform>(entity);
    EntityHandle parent = hierarchy.getParent();
    if (parent == entt::null) {
        hierarchy.setWorldMatrix(transform.getLocalMatrix());
    } else {
        Transform & parent_transform = m_registry_p->get<Transform>(parent);
        hierarchy.setWorldMatrix(parent_transform.getWorldMatrix() * transform.getLocalMatrix());
    }
    transform.setWorldMatrix(&hierarchy.getWorldMatrix());
    hierarchy.markClean();
    for (auto child : hierarchy.getChildren()) {
        this->updateRecursively(child, hierarchy.getWorldMatrix());
    }
}

void lcf::TransformSystem::updateRecursively(EntityHandle entity, const Matrix4x4 &parent_world_matrix) noexcept
{
    auto & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    auto & transform = m_registry_p->get<Transform>(entity);
    hierarchy.setWorldMatrix(parent_world_matrix * transform.getLocalMatrix());
    transform.setWorldMatrix(&hierarchy.getWorldMatrix());
    for (auto child : hierarchy.getChildren()) {
        this->updateRecursively(child, hierarchy.getWorldMatrix());
    }
    hierarchy.markClean();
}