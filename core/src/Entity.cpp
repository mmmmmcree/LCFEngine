#include "Entity.h"

lcf::Entity::Entity(Registry * registry_p) :
    m_registry_p(registry_p)
{
    m_entity = m_registry_p->create();
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
    m_registry_p->destroy(m_entity);
}