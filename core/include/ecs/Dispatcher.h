#pragma once

#include <entt/signal/dispatcher.hpp>
#include "type_traits/callable_traits.h"

namespace lcf::ecs {
    class Dispatcher : public entt::dispatcher
    {
        using Base = entt::dispatcher;
    public:
        using Base::Base;
    public:
        template<auto Candidate>
        entt::connection connect()
        {
            using Signal = std::decay_t<std::tuple_element_t<0, typename callable_traits<decltype(Candidate)>::arg_types>>;
            return this->sink<Signal>().connect<Candidate>();
        }
        template<auto Candidate, typename Type>
        entt::connection connect(Type & value_or_instance) 
        {
            using Signal = std::decay_t<std::tuple_element_t<0, typename callable_traits<decltype(Candidate)>::arg_types>>;
            return this->sink<Signal>().connect<Candidate>(value_or_instance);
        }
        template<auto Candidate>
        void disconnect()
        {
            using Signal = std::decay_t<std::tuple_element_t<0, typename callable_traits<decltype(Candidate)>::arg_types>>;
            this->sink<Signal>().disconnect<Candidate>();
        }
        template<auto Candidate, typename Type>
        void disconnect(Type & value_or_instance)
        {
            using Signal = std::decay_t<std::tuple_element_t<0, typename callable_traits<decltype(Candidate)>::arg_types>>;
            this->sink<Signal>().disconnect<Candidate>(value_or_instance);
        }
    private:
    };
}