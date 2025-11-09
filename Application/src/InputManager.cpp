#include "InputManager.h"

lcf::InputManager::InputManager(QObject *parent) : QObject(parent)
{
}

bool lcf::InputManager::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
        case QEvent::KeyPress: { this->handleKeyPressEvent(static_cast<QKeyEvent*>(event)); } break;
        case QEvent::KeyRelease: { this->handleKeyReleaseEvent(static_cast<QKeyEvent*>(event)); }  break;
        case QEvent::MouseButtonPress: { this->handleMousePressEvent(static_cast<QMouseEvent*>(event)); } break;
        case QEvent::MouseButtonRelease: { this->handleMouseReleaseEvent(static_cast<QMouseEvent*>(event)); } break;
        case QEvent::MouseMove: { this->handleMouseMoveEvent(static_cast<QMouseEvent*>(event)); } return true;
        case QEvent::Wheel: { this->handleWheelEvent(static_cast<QWheelEvent*>(event)); }  break;
    }
    return QObject::eventFilter(watched, event);
}

void lcf::InputManager::handleKeyPressEvent(QKeyEvent *event) noexcept
{
    if (event->isAutoRepeat()) { return; }
    this->getWritableState().m_pressed_keys.insert(static_cast<Key>(event->key()));
}

void lcf::InputManager::handleKeyReleaseEvent(QKeyEvent *event) noexcept
{
    if (event->isAutoRepeat()) { return; }
    this->getWritableState().m_pressed_keys.remove(static_cast<Key>(event->key()));
}

void lcf::InputManager::handleMousePressEvent(QMouseEvent *event) noexcept
{
    this->getWritableState().m_pressed_mouse_buttons |= event->buttons();
}

void lcf::InputManager::handleMouseReleaseEvent(QMouseEvent *event) noexcept
{ 
    this->getWritableState().m_pressed_mouse_buttons &= ~event->button();
}

void lcf::InputManager::handleMouseMoveEvent(QMouseEvent *event) noexcept
{
    auto [x, y] = event->position();
    this->getWritableState().m_current_mouse_pos = Vector2D<double>(x, y);
}

void lcf::InputManager::handleWheelEvent(QWheelEvent *event) noexcept
{
    auto [dx, dy] = event->angleDelta();
    this->getWritableState().m_wheel_delta += Vector2D<int>(dx, dy);
}

void lcf::InputManager::update() noexcept
{
    std::lock_guard lock(m_mutex);
    this->getReadableState() = this->getWritableState();
}

bool lcf::InputManager::isKeyPressed(Key key) const noexcept
{
    return this->getReadableState().m_pressed_keys.contains(key);
}

bool lcf::InputManager::isMouseButtonPressed(MouseButton button) const noexcept
{
    return this->getReadableState().m_pressed_mouse_buttons & button;
}

bool lcf::InputManager::isMouseButtonsPressed(MouseButtons buttons) const noexcept
{
    return this->getReadableState().m_pressed_mouse_buttons & buttons;
}

const lcf::Vector2D<double> & lcf::InputManager::getMousePosition() const noexcept
{
    return this->getReadableState().m_current_mouse_pos;
}

const lcf::Vector2D<int> &lcf::InputManager::getWheelDelta() const noexcept
{
    return this->getReadableState().m_wheel_delta;
}