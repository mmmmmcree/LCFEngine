#include "Entity.h"

lcf::Entity::Entity(Registry & registry) :
    m_registry_p(&registry),
    m_entity(registry.create())
{
}

lcf::Entity::Entity(Entity && other) noexcept :
    m_registry_p(other.m_registry_p),
    m_entity(std::exchange(other.m_entity, null_entity))
{
}

lcf::Entity & lcf::Entity::operator=(Entity && other) noexcept
{
    if (this == &other) { return *this; }
    this->destroy();
    m_registry_p = other.m_registry_p;
    m_entity = std::exchange(other.m_entity, null_entity); 
    return *this;
}

lcf::Entity::~Entity()
{
    this->destroy();
}

void lcf::Entity::setRegistry(Registry & registry)
{
    this->destroy();
    m_registry_p = &registry;
    m_entity = registry.create();
}

void lcf::Entity::destroy()
{
    if (not m_registry_p or m_entity == null_entity) { return; }
    m_registry_p->destroy(m_entity);
    m_registry_p = nullptr;
    m_entity = null_entity;
}
