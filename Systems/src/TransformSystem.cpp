#include "TransformSystem.h"
#include "signals.h"
#include <deque>
#include <algorithm>

lcf::TransformSystem::TransformSystem(Registry & registry) :
    m_registry_p(&registry)
{
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    dispatcher.sink<lcf::TransformAttachSignal>().connect<&lcf::TransformSystem::onTransformHierarchyAttach>(*this);
    dispatcher.sink<lcf::TransformDetachSignal>().connect<&lcf::TransformSystem::onTransformHierarchyDetach>(*this);
    dispatcher.sink<lcf::TransformUpdateSignal>().connect<&lcf::TransformSystem::onTransformUpdate>(*this);
}

lcf::TransformSystem::~TransformSystem()
{
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    dispatcher.sink<lcf::TransformAttachSignal>().disconnect(this);
    dispatcher.sink<lcf::TransformDetachSignal>().disconnect(this);
    dispatcher.sink<lcf::TransformUpdateSignal>().disconnect(this);
}

void lcf::TransformSystem::onTransformUpdate(const TransformUpdateSignal &info) noexcept
{
    this->markDirty(info.m_sender);
}

void lcf::TransformSystem::onTransformHierarchyAttach(const TransformAttachSignal &info)
{
    this->attach(info.m_parent, info.m_sender);    
}

void lcf::TransformSystem::onTransformHierarchyDetach(const TransformDetachSignal &info)
{
    this->detach(info.m_sender);
}

void lcf::TransformSystem::update() noexcept
{
    for (auto [entity, transform] : m_registry_p->view<Transform>().each()) {
        transform.getWorldMatrix();
    }
}

void lcf::TransformSystem::attach(EntityHandle parent, EntityHandle child)
{
    if (child == parent or child == null_entity_handle) { return; }
    auto & child_hierarchy = m_registry_p->get_or_emplace<TransformHierarchy>(child);
    auto & child_transform = m_registry_p->get_or_emplace<Transform>(child);
    if (parent == null_entity_handle) { return; }
    auto & parent_hierarchy = m_registry_p->get_or_emplace<TransformHierarchy>(parent);
    auto & parent_transform = m_registry_p->get_or_emplace<Transform>(parent);
    this->detach(child);
    child_transform.setParent(parent_transform);
    child_hierarchy.setParent(parent);
    parent_hierarchy.addChild(child);
    this->markDirty(child);
}

void lcf::TransformSystem::detach(EntityHandle entity)
{
    auto & transform = m_registry_p->get<Transform>(entity);
    auto & hierarchy = m_registry_p->get<TransformHierarchy>(entity);
    transform.setNullParent();
    auto & parent_hierarchy = m_registry_p->get<TransformHierarchy>(hierarchy.getParent());
    parent_hierarchy.removeChild(entity);
}

void lcf::TransformSystem::markDirty(EntityHandle entity) noexcept
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

void lcf::TransformHierarchy::removeChild(EntityHandle child)
{
    auto it = std::ranges::find(m_children, child);
    if (it != m_children.end()) {
        std::swap(*it, m_children.back());
        m_children.pop_back();
    }
}