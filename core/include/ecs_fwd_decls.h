#pragma once

#include <entt/entt.hpp>

namespace lcf {
    using EntityHandle = entt::entity;
    constexpr EntityHandle null_entity_handle = entt::null;
    class Entity;
    class Dispatcher;
    class Registry;
}