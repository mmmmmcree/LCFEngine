#pragma once

#include "input_enums.h"
#include <bitset>
#include <magic_enum/magic_enum_utility.hpp>
#include "Vector.h"

namespace lcf {
    class InputState
    {
    public:
        using PressedKeySet = std::bitset<magic_enum::enum_count<KeyboardKey>()>;
        using MousePosition = Vector2D<float>;
        using WheelOffset = Vector2D<int32_t>;
        InputState() = default;
        InputState(const InputState & other) = default;
        InputState(InputState && other) = default;
        InputState & operator=(const InputState & other) = default;
        InputState & operator=(InputState && other) = default;
        void pressKey(KeyboardKey key) noexcept;
        void releaseKey(KeyboardKey key) noexcept;
        bool isKeyPressed(KeyboardKey key) const noexcept;
        void pressMouseButtons(MouseButtonFlags buttons) noexcept;
        void releaseMouseButtons(MouseButtonFlags buttons) noexcept;
        bool isMouseButtonsPressed(MouseButtonFlags buttons) const noexcept;
        void setMousePosition(const MousePosition & position) noexcept { m_mouse_position = position; }
        void addMousePosition(const MousePosition & delta) noexcept { m_mouse_position += delta; }
        const MousePosition & getMousePosition() const noexcept { return m_mouse_position; }
        void addWheelOffset(const WheelOffset & delta) noexcept { m_wheel_offset += delta; }
        const WheelOffset & getWheelOffset() const noexcept { return m_wheel_offset; }
    private:
        PressedKeySet m_pressed_keys;
        MouseButtonFlags m_mouse_button_flags = MouseButtonFlags::eNoButton;
        MousePosition m_mouse_position;
        WheelOffset m_wheel_offset;
    };
}