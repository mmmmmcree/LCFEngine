#include "InputCollector.h"

using namespace lcf;

void InputCollector::collect(const InputState & input_state) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_input_state = input_state;
}

InputState InputCollector::getSnapshot() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_input_state;
}
