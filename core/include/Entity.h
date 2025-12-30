#pragma once

#include <entt/entt.hpp>
#include "Registry.h"
#include "tasks/TaskScheduler.h"

namespace lcf {
    using EntityHandle = entt::entity;
    constexpr EntityHandle null_entity = entt::null;

    class Entity
    {
    public:
        Entity() = default;
        Entity(Registry & registry);
        Entity(const Entity & other) = delete;
        Entity(Entity && other) noexcept;
        Entity & operator=(const Entity & other) = delete;
        Entity & operator=(Entity && other) noexcept;
        ~Entity();
        void create(Registry & registry);
        EntityHandle getHandle() const noexcept { return m_entity; }
        template <typename Component, typename... Args>
        Component & emplaceComponent(Args &&... args) const;
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
        void destroy();
    private:
        Registry * m_registry_p = nullptr;
        EntityHandle m_entity = entt::null;
    };

    struct EntitySignalBase
    {
        EntitySignalBase() = default;
        EntitySignalBase(EntityHandle entity) : m_sender(entity) {}
        EntityHandle m_sender = entt::null;
    };

    template <typename SignalInfo>
    concept entity_eignal_info_c = std::derived_from<SignalInfo, EntitySignalBase>;
}

template <typename Component, typename... Args>
inline Component &lcf::Entity::emplaceComponent(Args &&...args) const
{
    return m_registry_p->emplace<Component>(m_entity, std::forward<Args>(args)...);
}

template <typename Component, typename... Args>
inline Component &lcf::Entity::requireComponent(Args &&...args) const
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
    auto & task_scheduler = m_registry_p->ctx().get<TaskScheduler>();
    auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
    task_scheduler.post([&dispatcher, &signal_info]() {
        dispatcher.trigger(signal_info);
    });
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
