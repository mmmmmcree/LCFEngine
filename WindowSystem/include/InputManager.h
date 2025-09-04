#pragma once

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHash>

namespace lcf {
    class InputManager : QObject
    {
    public:
        using Key = Qt::Key;
        using MouseButton = Qt::MouseButton;
        using MouseButtons = Qt::MouseButtons;
        explicit InputManager(QObject *parent = nullptr);
        bool isKeyPressed(Key key) const { return m_pressed_keys.value(key, false); }
        bool isMouseButtonPressed(MouseButton button) const { return m_pressed_mouse_buttons & button; }
        bool isMouseButtonsPressed(MouseButtons buttons) const { return m_pressed_mouse_buttons & buttons; }
        QPointF getMousePosition() const { return m_current_mouse_pos; }
        QPointF getMouseDelta() const { return m_current_mouse_pos - m_last_mouse_pos; }
        QPoint getWheelDelta() const { return m_wheel_delta; }
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
        MouseButtons m_pressed_mouse_buttons = MouseButton::NoButton;
        QPointF m_current_mouse_pos, m_last_mouse_pos;
        QPoint m_wheel_delta;
    };
}