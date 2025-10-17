#pragma once

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHash>
#include <array>

#include "Vector.h"

namespace lcf {
    class InputManager : public QObject
    {
    public:
        class InputState;
        using Key = Qt::Key;
        using MouseButton = Qt::MouseButton;
        using MouseButtons = Qt::MouseButtons;
        explicit InputManager(QObject *parent = nullptr);
        InputManager(const InputManager &) = delete;
        InputManager(InputManager &&) = delete;
        InputManager & operator=(const InputManager &) = delete;
        InputManager & operator=(InputManager &&) = delete;
        void update() noexcept;
        bool isKeyPressed(Key key) const noexcept;
        bool isMouseButtonPressed(MouseButton button) const noexcept;
        bool isMouseButtonsPressed(MouseButtons buttons) const noexcept;
        const Vector2D_D & getMousePosition() const noexcept;
        const Vector2D_I & getWheelDelta() const noexcept;
    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
    private:
        void handleKeyPressEvent(QKeyEvent *event) noexcept;
        void handleKeyReleaseEvent(QKeyEvent *event) noexcept;
        void handleMousePressEvent(QMouseEvent *event) noexcept;
        void handleMouseReleaseEvent(QMouseEvent *event) noexcept;
        void handleMouseMoveEvent(QMouseEvent *event) noexcept;
        void handleWheelEvent(QWheelEvent *event) noexcept;
        InputState & getWritableState() noexcept { return m_input_states[0]; }
        InputState & getReadableState()  noexcept { return m_input_states[1]; }
        const InputState & getReadableState() const noexcept { return m_input_states[1]; }
    private:
        struct InputState
        {
            QSet<Key> m_pressed_keys;
            Vector2D_D m_current_mouse_pos;
            Vector2D_I m_wheel_delta;
            MouseButtons m_pressed_mouse_buttons = MouseButton::NoButton;
        };   
        std::array<InputState, 2> m_input_states;
    };
}