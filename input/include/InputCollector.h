#pragma once

#include "InputState.h"
#include <mutex>

namespace lcf {
    class InputCollector
    {
    public:
        void collect(const InputState & input_state) noexcept;
        InputState getSnapshot() const noexcept;
    private:
        mutable std::mutex m_mutex;
        InputState m_input_state;
    };
}