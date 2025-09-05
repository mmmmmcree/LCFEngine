#include "TrackballController.h"

void lcf::modules::TrackballController::update()
{
    if (not m_input_manager or not m_controlled_transform) { return; }
    float delta_yaw = 0.0f, delta_pitch = 0.0f, delta_front = 0.0f, delta_up = 0.0f, delta_right = 0.0f;
    auto current_mouse_pos = m_input_manager->getMousePosition();
    auto current_wheel_delta = m_input_manager->getWheelDelta();
    auto [dx, dy] = current_mouse_pos - m_last_mouse_pos;
    delta_front += (current_wheel_delta.y() - m_last_wheel_delta.y()) * m_zoom_speed;
    m_last_mouse_pos = current_mouse_pos;
    m_last_wheel_delta = current_wheel_delta;
    if (m_input_manager->isMouseButtonPressed(InputManager::MouseButton::LeftButton)) {
        delta_yaw -= dx * m_sensitivity;
        delta_pitch -= dy * m_sensitivity;
    }
    if (m_input_manager->isMouseButtonPressed(InputManager::MouseButton::RightButton)) {
        delta_right -= dx * m_move_speed;
        delta_up += dy * m_move_speed;
    }
    Vector3D right = m_controlled_transform->getXAxis();
    Vector3D up = m_controlled_transform->getYAxis();
    Vector3D delta_center = right * delta_right + up * delta_up;
    m_center += delta_center;
    Quaternion pitch = Quaternion::fromAxisAndAngle(right, delta_pitch);
    Quaternion yaw = Quaternion::fromAxisAndAngle({0.0f, 1.0f, 0.0f}, delta_yaw);
    m_controlled_transform->translateWorld(delta_center);
    m_controlled_transform->rotateAround(yaw * pitch, m_center); //-先转pitch再转yaw，否则转完yaw后camera->right()就变了，先前算的pitch失效
    m_controlled_transform->translateLocalZAxis(-delta_front);
}