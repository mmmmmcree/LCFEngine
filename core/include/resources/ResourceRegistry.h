#pragma once

#include "ecs/ecs_fwd_decls.h"
#include "ecs/Dispatcher.h"

namespace lcf {
    class ResourceRegistry : public ecs::BasicRegistry
    {
        using Base = ecs::BasicRegistry;
    public:
        using Base::Base;
        ResourceRegistry() = delete;
        ResourceRegistry(ecs::Dispatcher & dispatcher);
    public:
        template <typename Signal>
        void triggerSignal(Signal && signal) const
        {
            auto dispatcher_p = this->ctx().get<ecs::Dispatcher *>();
            dispatcher_p->trigger(std::forward<Signal>(signal));
        }
        template <typename Signal>
        void enqueueSignal(Signal && signal) const
        {
            auto dispatcher_p = this->ctx().get<ecs::Dispatcher *>();
            dispatcher_p->enqueue(std::forward<Signal>(signal));
        }
    };
}