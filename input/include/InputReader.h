#pragma once

#include "InputState.h"

namespace lcf {
    class InputReader
    {
    public:
        InputReader() = default;
        void update(InputState latest_state);
        const InputState & getCurrentState() const noexcept { return m_current_state; }
        const InputState & getPreviousState() const noexcept { return m_previous_state; }
    private:
        InputState m_current_state;
        InputState m_previous_state;
    };
}