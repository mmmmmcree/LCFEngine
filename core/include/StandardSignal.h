#pragma once

#include <entt/entt.hpp>

namespace lcf {
    template <typename _SignalInfo>
    class StandardSignal
    {
    public:
        using SignalInfo = _SignalInfo;
        using Signal = entt::sigh<void(const SignalInfo &)>;
        using Sink = entt::sink<Signal>;
        using Connection = entt::connection;
        StandardSignal() : m_sink(m_signal) {}
        void publish(const SignalInfo & info) { m_signal.publish(info); }
        template<auto Candidate>
        Connection connect() { m_sink.connect<Candidate>(); }
        template<auto Candidate, typename Type>
        Connection connect(Type & value_or_instance) { return m_sink.connect<Candidate>(value_or_instance); }
        template<auto Candidate>
        void disconnect() { m_sink.disconnect<Candidate>(); }
        template<auto Candidate, typename Type>
        void disconnect(Type & value_or_instance) { m_sink.disconnect<Candidate>(value_or_instance); }
    private: 
        Signal m_signal;
        Sink m_sink;
    };
}