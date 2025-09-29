#pragma once

#include <entt/entt.hpp>

namespace lcf {
    class Entity
    {
    public:
        using Registry = entt::registry;
        using EntityHandle = entt::entity;
        Entity(Registry * registry = nullptr);
        Entity(const Entity & other);
        Entity(Entity && other);
        Entity & operator=(const Entity & other);
        Entity & operator=(Entity && other);
        ~Entity();
        EntityHandle getHandle() const { return m_entity; }
        template <typename Component, typename... Args>
        Component & requireComponent(Args &&... args) const;
        template <typename Component>
        Component & getComponent() const;
        template <typename Component>
        bool hasComponent() const;
    private:
        Registry * m_registry = nullptr;
        EntityHandle m_entity;
        bool m_owns_entity = false;
        bool m_owns_registry = false;
    };
}

template <typename Component, typename... Args>
inline Component & lcf::Entity::requireComponent(Args &&...args) const
{
    return m_registry->get_or_emplace<Component>(m_entity, std::forward<Args>(args)...);
}

template <typename Component>
inline Component & lcf::Entity::getComponent() const
{
    return m_registry->get<Component>(m_entity);
}

template<typename Component>
inline bool lcf::Entity::hasComponent() const
{
    return m_registry->any_of<Component>(m_entity);
}
