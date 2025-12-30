#include "InputCollector.h"

using namespace lcf;

void InputCollector::collect(const InputState &input_state) noexcept
{
    m_input_state.write(input_state);
}

const InputState & InputCollector::getSnapshot() const noexcept
{
    m_input_state.read(m_cached_input_state);
    return m_cached_input_state;
}
