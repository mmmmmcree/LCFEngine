#pragma once

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHash>
#include "Vector.h"

namespace lcf {
    class InputManager : QObject
    {
    public:
        using Key = Qt::Key;
        using MouseButton = Qt::MouseButton;
        using MouseButtons = Qt::MouseButtons;
        explicit InputManager(QObject *parent = nullptr);
        InputManager(const InputManager &) = delete;
        InputManager(InputManager &&) = delete;
        InputManager & operator=(const InputManager &) = delete;
        InputManager & operator=(InputManager &&) = delete;
        bool isKeyPressed(Key key) const { return m_pressed_keys.value(key, false); }
        bool isMouseButtonPressed(MouseButton button) const { return m_pressed_mouse_buttons & button; }
        bool isMouseButtonsPressed(MouseButtons buttons) const { return m_pressed_mouse_buttons & buttons; }
        const Vector2D_D & getMousePosition() const { return m_current_mouse_pos; }
        const Vector2D_I & getWheelDelta() const { return m_wheel_delta; }
    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
    private:
        void handleKeyPressEvent(QKeyEvent *event);
        void handleKeyReleaseEvent(QKeyEvent *event);
        void handleMousePressEvent(QMouseEvent *event);
        void handleMouseReleaseEvent(QMouseEvent *event);
        void handleMouseMoveEvent(QMouseEvent *event);
        void handleWheelEvent(QWheelEvent *event);
    private:
        QHash<Key, bool> m_pressed_keys;
        Vector2D_D m_current_mouse_pos;
        Vector2D_I m_wheel_delta;
        MouseButtons m_pressed_mouse_buttons = MouseButton::NoButton;
    };
}