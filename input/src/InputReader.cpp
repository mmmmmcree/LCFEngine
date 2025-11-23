#include "InputReader.h"
#include <utility>

using namespace lcf;

void InputReader::update(InputState latest_state)
{
    m_previous_state = std::exchange(m_current_state, std::move(latest_state));
}