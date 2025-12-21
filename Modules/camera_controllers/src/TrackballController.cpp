#include "TrackballController.h"
#include "InputReader.h"
#include "Transform.h"
#include "signals.h"

void lcf::modules::TrackballController::update(Entity & camera, float delta_time)
{
    if (not m_input_reader) { return; }
    float delta_yaw = 0.0f, delta_pitch = 0.0f, delta_front = 0.0f, delta_up = 0.0f, delta_right = 0.0f;
    const auto & current_input_state = m_input_reader->getCurrentState();
    const auto & prev_input_state = m_input_reader->getPreviousState();

    auto [dx, dy] = current_input_state.getMousePosition() - prev_input_state.getMousePosition();
    auto delta_wheel_offset = current_input_state.getWheelOffset() - prev_input_state.getWheelOffset();
    delta_front = (delta_wheel_offset.y) * m_zoom_speed * delta_time;
    
    if (current_input_state.isMouseButtonsPressed(MouseButtonFlags::eLeftButton)) {
        delta_yaw -= dx * m_sensitivity;
        delta_pitch -= dy * m_sensitivity;
    }
    if (current_input_state.isMouseButtonsPressed(MouseButtonFlags::eRightButton)) {
        delta_right -= dx * m_move_speed;
        delta_up += dy * m_move_speed;
    }

    auto & camera_transform = camera.getComponent<Transform>();
    auto right = camera_transform.getXAxis().normalized();
    auto up = camera_transform.getYAxis().normalized();
    auto delta_center = (right * delta_right + up * delta_up) * delta_time;
    m_center += delta_center;
    auto pitch = Quaternion::fromAxisAndAngle(right, delta_pitch);
    auto yaw = Quaternion::fromAxisAndAngle({0.0f, 1.0f, 0.0f}, delta_yaw);
    camera_transform.translateWorld(delta_center);
    camera_transform.rotateAround(yaw * pitch, m_center); //-先转pitch再转yaw，否则转完yaw后camera->right()就变了，先前算的pitch失效
    camera_transform.translateLocalZAxis(-delta_front);
    camera.emitSignal<lcf::TransformUpdateSignal>({});
}
