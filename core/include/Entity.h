#pragma once

#include <entt/entt.hpp>
#include "Registry.h"

namespace lcf {
    class Registry;
    using Dispatcher = entt::dispatcher;
    using EntityHandle = entt::entity;

    class Entity
    {
    public:
        Entity(Registry * registry_p);
        Entity(const Entity & other) = delete;
        Entity(Entity && other) noexcept;
        Entity & operator=(const Entity & other) = delete;
        Entity & operator=(Entity && other) noexcept;
        ~Entity();
        EntityHandle getHandle() const { return m_entity; }
        template <typename Component, typename... Args>
        Component & requireComponent(Args &&... args) const;
        template <typename Component>
        Component & getComponent() const;
        template <typename Component>
        bool hasComponent() const;
        template <typename SignalInfo>
        void emitSignal(const SignalInfo & signal_info) const;
        template <typename SignalInfo>
        void enqueueSignal(const SignalInfo & signal_info) const;
    private:
        Registry * m_registry_p = nullptr;
        EntityHandle m_entity;
    };

    struct EntitySignalInfoBase
    {
        EntitySignalInfoBase() = default;
        EntitySignalInfoBase(EntityHandle entity) : m_sender(entity) {}
        EntityHandle m_sender = entt::null;
    };

    template <typename SignalInfo>
    concept entity_eignal_info_c = std::derived_from<SignalInfo, EntitySignalInfoBase>;
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

template <typename SignalInfo>
inline void lcf::Entity::emitSignal(const SignalInfo &signal_info) const
{
    if constexpr (entity_eignal_info_c<SignalInfo>) {
        SignalInfo & info = const_cast<SignalInfo &>(signal_info);
        info.m_sender = m_entity;
    }
    m_registry_p->ctx().get<Dispatcher>().trigger(signal_info);
}

template <typename SignalInfo>
inline void lcf::Entity::enqueueSignal(const SignalInfo &signal_info) const
{
    if constexpr (entity_eignal_info_c<SignalInfo>) {
        SignalInfo & info = const_cast<SignalInfo &>(signal_info);
        info.m_sender = m_entity;
    }
    m_registry_p->ctx().get<Dispatcher>().enqueue(signal_info);
}
