#include "ecs/Entity.h"

using namespace lcf::ecs;

Entity::Entity(BasicRegistry & registry) :
    m_registry_p(&registry),
    m_id(registry.create())
{
}

Entity::Entity(Entity && other) noexcept :
    m_registry_p(other.m_registry_p),
    m_id(std::exchange(other.m_id, null))
{
}

Entity & Entity::operator=(Entity && other) noexcept
{
    if (this == &other) { return *this; }
    this->destroy();
    m_registry_p = other.m_registry_p;
    m_id = std::exchange(other.m_id, null); 
    return *this;
}

Entity::~Entity()
{
    this->destroy();
}

void Entity::create(BasicRegistry & registry)
{
    this->destroy();
    m_registry_p = &registry;
    m_id = registry.create();
}

void Entity::destroy()
{
    if (not m_registry_p or m_id == null) { return; }
    m_registry_p->destroy(m_id);
    m_registry_p = nullptr;
    m_id = null;
}
