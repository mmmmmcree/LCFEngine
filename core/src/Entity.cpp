#include "Entity.h"

lcf::Entity::Entity(Registry & registry) :
    m_registry_p(&registry),
    m_entity(registry.create())
{
}

lcf::Entity::Entity(Entity && other) noexcept :
    m_registry_p(other.m_registry_p),
    m_entity(std::move(other.m_entity))
{
}

lcf::Entity & lcf::Entity::operator=(Entity && other) noexcept
{
    m_registry_p = other.m_registry_p;
    m_entity = std::move(other.m_entity);
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
    if (not m_registry_p) { return; }
    m_registry_p->destroy(m_entity);
    m_registry_p = nullptr;
    m_entity = entt::null;
}
