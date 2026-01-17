#pragma once

#include <entt/entt.hpp>

namespace lcf {
    using EntityHandle = entt::entity;
    constexpr EntityHandle null_entity_handle = entt::null;
    using Dispatcher = entt::dispatcher;
}