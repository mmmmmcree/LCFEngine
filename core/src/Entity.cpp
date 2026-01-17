#include "Entity.h"

using namespace lcf;

Entity::Entity(Registry & registry) :
    m_registry_p(&registry),
    m_entity(registry.create())
{
}

Entity::Entity(Entity && other) noexcept :
    m_registry_p(other.m_registry_p),
    m_entity(std::exchange(other.m_entity, null_entity_handle))
{
}

Entity & Entity::operator=(Entity && other) noexcept
{
    if (this == &other) { return *this; }
    this->destroy();
    m_registry_p = other.m_registry_p;
    m_entity = std::exchange(other.m_entity, null_entity_handle); 
    return *this;
}

Entity::~Entity()
{
    this->destroy();
}

void Entity::create(Registry & registry)
{
    this->destroy();
    m_registry_p = &registry;
    m_entity = registry.create();
}

void Entity::destroy()
{
    if (not m_registry_p or m_entity == null_entity_handle) { return; }
    m_registry_p->destroy(m_entity);
    m_registry_p = nullptr;
    m_entity = null_entity_handle;
}
