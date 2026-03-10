#include "TransformSystem.h"
#include "ecs/Registry.h"
#include "ecs/signals.h"
#include <deque>
#include <algorithm>

using namespace lcf::ecs;

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

void TransformSystem::onTransformUpdate(const TransformUpdateSignal &info) noexcept
{
    this->markDirty(info.m_sender);
}

void TransformSystem::onTransformHierarchyAttach(const TransformAttachSignal &info)
{
    this->attach(info.m_parent, info.m_sender);    
}

void TransformSystem::onTransformHierarchyDetach(const TransformDetachSignal &info)
{
    this->detach(info.m_sender);
}

void TransformSystem::update() noexcept
{
    for (auto [entity, transform] : m_registry_p->view<Transform>().each()) {
        transform.getWorldMatrix();
    }
}

void TransformSystem::attach(EntityId parent, EntityId child)
{
    if (child == parent or child == null) { return; }
    auto & child_hierarchy = m_registry_p->get_or_emplace<TransformHierarchy>(child);
    auto & child_transform = m_registry_p->get_or_emplace<Transform>(child);
    if (parent == null) { return; }
    auto & parent_hierarchy = m_registry_p->get_or_emplace<TransformHierarchy>(parent);
    auto & parent_transform = m_registry_p->get_or_emplace<Transform>(parent);
    this->detach(child);
    child_transform.setParent(parent_transform);
    child_hierarchy.setParent(parent);
    parent_hierarchy.addChild(child);
    this->markDirty(child);
}

void TransformSystem::detach(EntityId entity)
{
    auto & transform = m_registry_p->get<Transform>(entity);
    auto & hierarchy = m_registry_p->get<TransformHierarchy>(entity);
    transform.setNullParent();
    auto & parent_hierarchy = m_registry_p->get<TransformHierarchy>(hierarchy.getParent());
    parent_hierarchy.removeChild(entity);
}

void TransformSystem::markDirty(EntityId entity) noexcept
{
    auto & transform = m_registry_p->get<Transform>(entity);
    if (transform.isDirty()) { return; }
    transform.markDirty();
    auto * inverted_matrix = m_registry_p->try_get<TransformInvertedWorldMatrix>(entity);
    if (inverted_matrix) { inverted_matrix->markDirty(); }
    auto * hierarchy = m_registry_p->try_get<TransformHierarchy>(entity);
    if (not hierarchy) { return; }
    for (auto child : hierarchy->getChildren()) {
        this->markDirty(child);
    }
}

void TransformHierarchy::removeChild(EntityId child)
{
    auto it = std::ranges::find(m_children, child);
    if (it != m_children.end()) {
        std::swap(*it, m_children.back());
        m_children.pop_back();
    }
}