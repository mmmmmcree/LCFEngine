#include "TransformSystem.h"
#include "signals.h"

lcf::TransformSystem::TransformSystem(Registry *registry_p) :
    m_registry_p(registry_p)
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

void lcf::TransformSystem::update()
{
    for (uint16_t level = 0; level < s_frequent_level_count; ++level) {
        auto & dirty_entitys = m_frequent_level_map[level];
        for (auto entity : dirty_entitys) {
            this->updateDirtyEntity(entity);
        }
        dirty_entitys.clear();
    }
    for (auto &[level, entities] : m_unfrequent_level_map) {
        for (auto entity : entities) {
            this->updateDirtyEntity(entity);
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
    child_transform.setWorldMatrix(&child_hierarchy.getWorldMatrix());
    m_hierarchical_to_entity_map[&child_hierarchy] = child;
    if (parent == entt::null) { return; }
    auto & parent_hierarchy = m_registry_p->get_or_emplace<lcf::HierarchicalTransform>(parent);
    auto & parent_transform = m_registry_p->get_or_emplace<lcf::Transform>(parent);
    parent_transform.setWorldMatrix(&parent_hierarchy.getWorldMatrix());
    m_hierarchical_to_entity_map[&parent_hierarchy] = parent;
    child_hierarchy.setParent(&parent_hierarchy);
    this->markDirty(child, child_hierarchy);
}

void lcf::TransformSystem::detach(EntityHandle entity)
{
    auto & transform = m_registry_p->get<Transform>(entity);
    transform.setWorldMatrix(&transform.getLocalMatrix());
    auto & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    hierarchy.setParent(nullptr);
    m_hierarchical_to_entity_map.erase(&hierarchy);
}

void lcf::TransformSystem::markDirty(EntityHandle entity, HierarchicalTransform &hierarchy)
{
    if (m_dirty_entities.contains(entity)) { return; }
    m_dirty_entities.push(entity);
    uint16_t level = hierarchy.getLevel();
    if (level < s_frequent_level_count) {
        m_frequent_level_map[level].push_back(entity);
    } else {
        m_unfrequent_level_map[level].push_back(entity);
    }
    hierarchy.markDirty();
}

void lcf::TransformSystem::updateDirtyEntity(EntityHandle entity)
{
    HierarchicalTransform & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    if (not hierarchy.isDirty()) { return; } //- the return condition shows that the entity is updated by their parent
    Transform & transform = m_registry_p->get<Transform>(entity);
    EntityHandle parent = entt::null;
    auto parent_it = m_hierarchical_to_entity_map.find(hierarchy.getParent());
    if (parent_it != m_hierarchical_to_entity_map.end()) { parent = parent_it->second; }
    if (parent == entt::null) {
        hierarchy.setWorldMatrix(transform.getLocalMatrix());
    } else {
        Transform & parent_transform = m_registry_p->get<Transform>(parent);
        hierarchy.setWorldMatrix(parent_transform.getWorldMatrix() * transform.getLocalMatrix());
    }
    hierarchy.markClean();
    for (auto child : hierarchy.getChildren()) {
        this->updateRecursive(m_hierarchical_to_entity_map[child], hierarchy.getWorldMatrix());
    }
}

void lcf::TransformSystem::updateRecursive(EntityHandle entity, const Matrix4x4 &parent_world_matrix)
{
    auto & hierarchy = m_registry_p->get<HierarchicalTransform>(entity);
    auto & transform = m_registry_p->get<Transform>(entity);
    hierarchy.setWorldMatrix(parent_world_matrix * transform.getLocalMatrix());
    for (auto child : hierarchy.getChildren()) {
        this->updateRecursive(m_hierarchical_to_entity_map[child], hierarchy.getWorldMatrix());
    }
}
