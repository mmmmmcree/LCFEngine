#pragma once

#include "ecs_fwd_decls.h"

namespace lcf::ecs {
    class Entity
    {
    public:
        Entity() = default;
        Entity(BasicRegistry & registry);
        Entity(const Entity & other) = delete;
        Entity(Entity && other) noexcept;
        Entity & operator=(const Entity & other) = delete;
        Entity & operator=(Entity && other) noexcept;
        ~Entity();
        void create(BasicRegistry & registry);
        EntityId getId() const noexcept { return m_id; }
        template <typename Component, typename... Args>
        Component & emplaceComponent(Args &&... args) const
        {
            return m_registry_p->emplace<Component>(m_id, std::forward<Args>(args)...);
        }
        template <typename Component, typename... Args>
        Component & requireComponent(Args &&... args) const noexcept
        {
            return m_registry_p->get_or_emplace<Component>(m_id, std::forward<Args>(args)...);
        }
        template <typename Component>
        Component & getComponent() const
        {
            return m_registry_p->get<Component>(m_id);
        }
        template <typename Component>
        bool hasComponent() const noexcept
        {
            return m_registry_p->any_of<Component>(m_id);
        }
    private:
        void destroy();
    private:
        BasicRegistry * m_registry_p = nullptr;
        EntityId m_id = entt::null;
    };
}