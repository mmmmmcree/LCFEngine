#pragma once

#include "InputState.h"
#include <boost/lockfree/spsc_value.hpp>

namespace lcf {
    class InputCollector
    {
        using LockfreeInputState = boost::lockfree::spsc_value<InputState>;
    public:
        void collect(const InputState & input_state) noexcept;
        const InputState & getSnapshot() const noexcept;
    private:
        mutable LockfreeInputState m_input_state;
        mutable InputState m_cached_input_state;
    };
}