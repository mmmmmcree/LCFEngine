#include "TransformSystem.h"
#include "ecs/signals.h"
#include "ecs/Registry.h"
#include <deque>
#include <algorithm>
#include "Matrix.h"
#include "Vector.h"

using namespace lcf::ecs;

using Transform = ENTTTransform;

TransformSystem::TransformSystem(Registry & registry) :
    m_registry_p(&registry)
{
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    dispatcher.sink<TransformAttachSignal>().connect<&TransformSystem::onTransformHierarchyAttach>(*this);
    dispatcher.sink<TransformDetachSignal>().connect<&TransformSystem::onTransformHierarchyDetach>(*this);
    dispatcher.sink<TransformUpdateSignal>().connect<&TransformSystem::onTransformUpdate>(*this);
}

TransformSystem::~TransformSystem()
{
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    dispatcher.sink<TransformAttachSignal>().disconnect(this);
    dispatcher.sink<TransformDetachSignal>().disconnect(this);
    dispatcher.sink<TransformUpdateSignal>().disconnect(this);
}

void TransformSystem::onTransformUpdate(const TransformUpdateSignal &info)
{
    this->markDirty(info.m_sender);
}

void TransformSystem::onTransformHierarchyAttach(const TransformAttachSignal&info)
{
    this->attach(info.m_parent, info.m_sender);    
}

void TransformSystem::onTransformHierarchyDetach(const TransformDetachSignal &info)
{
    this->detach(info.m_sender);
}

void TransformSystem::update() noexcept
{
    for (uint32_t level = 0; level < s_frequent_level_count; ++level) {
        auto & dirty_entitys = m_frequent_level_map[level];
        for (auto entity : dirty_entitys) {
            this->updateDFS(entity);
        }
        dirty_entitys.clear();
    }
    for (auto [level, entities] : m_unfrequent_level_map) {
        for (auto entity : entities) {
            this->updateDFS(entity);
        }
    }
    m_unfrequent_level_map.clear();
    m_dirty_entities.clear();
}

void TransformSystem::attach(EntityId parent, EntityId child)
{
    if (child == parent or child == entt::null) { return; }
    auto & child_hierarchy = m_registry_p->get_or_emplace<TransformHierarchy>(child);
    auto & child_transform = m_registry_p->get_or_emplace<Transform>(child);
    auto & child_state = m_registry_p->get_or_emplace<TransformState>(child);
    if (parent == entt::null or parent == child_hierarchy.getParent()) { return; }
    auto & parent_hierarchy = m_registry_p->get_or_emplace<TransformHierarchy>(parent);
    auto & parent_transform = m_registry_p->get_or_emplace<Transform>(parent);
    auto & parent_state = m_registry_p->get_or_emplace<TransformState>(parent);
    child_hierarchy.setParent(parent);
    parent_hierarchy.addChild(child);
    child_hierarchy.setLevel(parent_hierarchy.getLevel() + 1);
    child_transform.setWorldMatrix(parent_transform.getWorldMatrix() * child_transform.getLocalMatrix());
}

void TransformSystem::detach(EntityId entity)
{
    auto & transform = m_registry_p->get<Transform>(entity);
    auto & hierarchy = m_registry_p->get<TransformHierarchy>(entity);
    hierarchy.setParent(null);
    auto & parent_hierarchy = m_registry_p->get<TransformHierarchy>(hierarchy.getParent());
    auto to_remove_it = std::ranges::find(parent_hierarchy.m_children, entity);
    if (to_remove_it != parent_hierarchy.m_children.end()) {
        std::swap(*to_remove_it, parent_hierarchy.m_children.back());
        parent_hierarchy.m_children.pop_back();
    }
}

void TransformSystem::markDirty(EntityId entity) noexcept
{
    auto * hierarchy_p = m_registry_p->try_get<TransformHierarchy>(entity);
    if (not hierarchy_p) { return; }
    if (m_dirty_entities.contains(entity)) { return; }
    m_dirty_entities.push(entity);
    auto level = hierarchy_p->getLevel();
    if (level < s_frequent_level_count) {
        m_frequent_level_map[level].emplace_back(entity);
    } else {
        m_unfrequent_level_map[level].emplace_back(entity);
    }
    auto & state = m_registry_p->get<TransformState>(entity);
    state.markDirty();
}

void TransformSystem::updateBFS(EntityId entity) noexcept
{
    auto & state = m_registry_p->get<TransformState>(entity);
    if (not state.isDirty()) { return; } //- the return condition shows that the entity is updated by their parent
    state.clean();
    auto & hierarchy = m_registry_p->get<TransformHierarchy>(entity);
    auto & transform = m_registry_p->get<Transform>(entity);
    EntityId parent = hierarchy.getParent();
    if (parent == entt::null) {
        transform.setWorldMatrix(transform.getLocalMatrix());
    } else {
        Transform & parent_transform = m_registry_p->get<Transform>(parent);
        transform.setWorldMatrix(parent_transform.getWorldMatrix() * transform.getLocalMatrix());
    }
    std::deque<EntityId> bfs_queue;
    bfs_queue.emplace_back(entity);
    while (not bfs_queue.empty()) {
        auto cur_entity = bfs_queue.front();
        bfs_queue.pop_front();
        auto & cur_hierarchy = m_registry_p->get<TransformHierarchy>(cur_entity);
        auto & cur_transform = m_registry_p->get<Transform>(cur_entity);
        for (auto child : cur_hierarchy.getChildren()) {
            auto & child_transform = m_registry_p->get<Transform>(child);
            child_transform.setWorldMatrix(cur_transform.getWorldMatrix() * child_transform.getLocalMatrix());
        }
        for (auto child : cur_hierarchy.getChildren()) {
            auto & child_state = m_registry_p->get<TransformState>(child);
            child_state.clean();
        }
        for (auto child : cur_hierarchy.getChildren()) {
            auto & child_hierarchy = m_registry_p->get<TransformHierarchy>(child);
            bfs_queue.emplace_back(child);
        }
    }
}

void TransformSystem::updateDFS(EntityId entity) noexcept
{
    auto & state = m_registry_p->get<TransformState>(entity);
    if (not state.isDirty()) { return; } //- the return condition shows that the entity is updated by their parent
    state.clean();
    auto & hierarchy = m_registry_p->get<TransformHierarchy>(entity);
    auto & transform = m_registry_p->get<Transform>(entity);
    EntityId parent = hierarchy.getParent();
    if (parent == entt::null) {
        transform.setWorldMatrix(transform.getLocalMatrix());
    } else {
        Transform & parent_transform = m_registry_p->get<Transform>(parent);
        transform.setWorldMatrix(parent_transform.getWorldMatrix() * transform.getLocalMatrix());
    }
    this->updateRecursively(entity);
}

void TransformSystem::updateRecursively(EntityId entity) noexcept
{
    auto & hierarchy = m_registry_p->get<TransformHierarchy>(entity);
    auto & transform = m_registry_p->get<Transform>(entity);
    for (auto child : hierarchy.getChildren()) {
        auto & child_transform = m_registry_p->get<Transform>(child);
        child_transform.setWorldMatrix(transform.getWorldMatrix() * child_transform.getLocalMatrix());
    }
    for (auto child : hierarchy.getChildren()) {
        this->updateRecursively(child);
    }
    for (auto child : hierarchy.getChildren()) {
        auto & child_state = m_registry_p->get<TransformState>(child);
        child_state.clean();
    }
}