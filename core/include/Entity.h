#pragma once

#include <entt/entt.hpp>

namespace lcf {
    using Registry = entt::registry;
    using EntityHandle = entt::entity;

    class Entity
    {
    public:
        Entity(Registry * registry_p, Entity * signal_manager = nullptr);
        Entity(const Entity & other) = delete;
        Entity(Entity && other) noexcept;
        Entity & operator=(const Entity & other) = delete;
        Entity & operator=(Entity && other) noexcept;
        ~Entity();
        void setSignalManager(Entity * signal_manager_p) { m_signal_manager_p = signal_manager_p; }
        EntityHandle getHandle() const { return m_entity; }
        template <typename Component, typename... Args>
        Component & requireComponent(Args &&... args) const;
        template <typename Component>
        Component & getComponent() const;
        template <typename Component>
        bool hasComponent() const;
        Entity * getSignalManager() const { return m_signal_manager_p; }
        template <typename Signal>
        void emitSignal(const typename Signal::SignalInfo & signal_info) const;
    private:
        Registry * m_registry_p = nullptr;
        Entity * m_signal_manager_p = nullptr; //optional
        EntityHandle m_entity;
    };

    struct EntitySignalInfoBase
    {
        EntitySignalInfoBase() = default;
        EntitySignalInfoBase(EntityHandle entity) : sender(entity) {}
        EntityHandle sender = entt::null;
    };

    template <typename Signal>
    concept entity_eignal_c = std::derived_from<typename Signal::SignalInfo, EntitySignalInfoBase>;
}

template <typename Component, typename... Args>
inline Component & lcf::Entity::requireComponent(Args &&...args) const
{
    return m_registry_p->get_or_emplace<Component>(m_entity, std::forward<Args>(args)...);
}

template <typename Component>
inline Component & lcf::Entity::getComponent() const
{
    return m_registry_p->get<Component>(m_entity);
}

template<typename Component>
inline bool lcf::Entity::hasComponent() const
{
    return m_registry_p->any_of<Component>(m_entity);
}

template <typename Signal>
inline void lcf::Entity::emitSignal(const typename Signal::SignalInfo &signal_info) const
{
    using SignalInfo = typename Signal::SignalInfo;
    if (m_signal_manager_p) {
        if constexpr (entity_eignal_c<Signal>) {
            SignalInfo & info = const_cast<SignalInfo &>(signal_info);
            info.sender = m_entity;
        }
        m_signal_manager_p->requireComponent<Signal>().publish(signal_info);
    }
}
