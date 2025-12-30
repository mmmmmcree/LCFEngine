#pragma once

#include <entt/entt.hpp>

namespace lcf {
    using Dispatcher = entt::dispatcher;

    class Registry : public entt::registry
    {
        using Base = entt::registry;
    public:
        Registry();
        using Base::Base;
    };
}