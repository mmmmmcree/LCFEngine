#include "InputManager.h"

lcf::InputManager::InputManager(QObject *parent) : QObject(parent)
{
    if (parent) {
        parent->installEventFilter(this);
    }
}

bool lcf::InputManager::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
        case QEvent::KeyPress: { this->handleKeyPressEvent(static_cast<QKeyEvent*>(event)); }  break;
        case QEvent::KeyRelease: { this->handleKeyReleaseEvent(static_cast<QKeyEvent*>(event)); }  break;
        case QEvent::MouseButtonPress: { this->handleMousePressEvent(static_cast<QMouseEvent*>(event)); }  break;
        case QEvent::MouseButtonRelease: { this->handleMouseReleaseEvent(static_cast<QMouseEvent*>(event)); }  break;
        case QEvent::MouseMove: { this->handleMouseMoveEvent(static_cast<QMouseEvent*>(event)); }  break;
        case QEvent::Wheel: { this->handleWheelEvent(static_cast<QWheelEvent*>(event)); }  break;
    }
    return QObject::eventFilter(watched, event);
}

void lcf::InputManager::handleKeyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) { return; }
    m_pressed_keys[static_cast<Key>(event->key())] = true;
}

void lcf::InputManager::handleKeyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) { return; }
    m_pressed_keys[static_cast<Key>(event->key())] = false;
}

void lcf::InputManager::handleMousePressEvent(QMouseEvent *event)
{
    m_pressed_mouse_buttons |= event->buttons();
}

void lcf::InputManager::handleMouseReleaseEvent(QMouseEvent *event)
{ 
    m_pressed_mouse_buttons &= ~event->button();
}

void lcf::InputManager::handleMouseMoveEvent(QMouseEvent *event)
{
    auto [x, y] = event->position();
    m_current_mouse_pos = Vector2D_D(x, y);
}

void lcf::InputManager::handleWheelEvent(QWheelEvent *event)
{
    auto [dx, dy] = event->angleDelta();
    m_wheel_delta += Vector2D_I(dx, dy);
}
