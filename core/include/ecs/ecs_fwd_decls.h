#pragma once

#include <entt/entt.hpp>

namespace lcf::ecs {
    using EntityId = entt::entity;

    constexpr EntityId null = entt::null;

    using BasicRegistry = entt::registry;

    class Entity;

    class Dispatcher;

    class Registry;
}