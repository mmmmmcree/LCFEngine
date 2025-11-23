#include "InputState.h"
#include "enum_cast.h"

void lcf::InputState::pressKey(KeyboardKey key) noexcept
{
    m_pressed_keys.set(to_integral(key));
}

void lcf::InputState::releaseKey(KeyboardKey key) noexcept
{
    m_pressed_keys.reset(to_integral(key));
}

bool lcf::InputState::isKeyPressed(KeyboardKey key) const noexcept
{
    return m_pressed_keys.test(to_integral(key));
}

void lcf::InputState::pressMouseButtons(MouseButtonFlags buttons) noexcept
{
    m_mouse_button_flags |= buttons;
}

void lcf::InputState::releaseMouseButtons(MouseButtonFlags buttons) noexcept
{
    m_mouse_button_flags &= ~buttons;
}

bool lcf::InputState::isMouseButtonsPressed(MouseButtonFlags buttons) const noexcept
{
    return (m_mouse_button_flags & buttons) == buttons;
}
