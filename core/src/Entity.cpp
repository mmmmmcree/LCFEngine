#include "Entity.h"

lcf::Entity::Entity(Registry *registry)
{
    m_registry = registry;
    if (not m_registry) {
        m_registry = new Registry;
        m_owns_registry = true;
    }
    m_entity = m_registry->create();
    m_owns_entity = true;
}

lcf::Entity::~Entity()
{
    if (m_owns_entity) {
        m_registry->destroy(m_entity);
    }
    if (m_owns_registry) {
        delete m_registry;
    }
}


lcf::Entity::Entity(const Entity &other) :
    m_registry(other.m_registry),
    m_entity(other.m_entity),
    m_owns_registry(false),
    m_owns_entity(false)
{
}

lcf::Entity::Entity(Entity &&other) :
    m_registry(other.m_registry),
    m_entity(other.m_entity),
    m_owns_registry(other.m_owns_registry),
    m_owns_entity(other.m_owns_entity)
{
    other.m_owns_entity = false;
    other.m_owns_registry = false;
}

lcf::Entity &lcf::Entity::operator=(const Entity &other)
{
    if (this == &other) { return *this; }
    m_registry = other.m_registry;
    m_entity = other.m_entity;
    m_owns_registry = false;
    m_owns_entity = false;
    return *this;
}

lcf::Entity &lcf::Entity::operator=(Entity &&other)
{
    if (this == &other) { return *this; }
    m_registry = other.m_registry;
    m_entity = other.m_entity;
    m_owns_registry = other.m_owns_registry;
    m_owns_entity = other.m_owns_entity;
    other.m_owns_entity = false;
    other.m_owns_registry = false;
    return *this;
}
