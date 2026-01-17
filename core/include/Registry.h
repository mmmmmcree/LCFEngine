#pragma once

#include "ecs_fwd_decls.h"

namespace lcf {
    class Registry : public entt::registry
    {
        using Base = entt::registry;
    public:
        Registry();
        using Base::Base;
    };
}