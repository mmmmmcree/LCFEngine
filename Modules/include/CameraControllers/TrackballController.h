#pragma once

#include "Transform.h"
#include "InputManager.h"

namespace lcf::modules
{
    class TrackballController
    {
    public:
        TrackballController() = default;
        void setInputManager(InputManager * input_manager) { m_input_manager = input_manager; }
        void controls(Transform & transform) { m_controlled_transform = &transform; }
        void controns(Transform * transform) { m_controlled_transform = transform; }
        void update();
    private:
        InputManager * m_input_manager = nullptr;
        Transform * m_controlled_transform = nullptr;
        float m_sensitivity = 0.2f;
        float m_move_speed = 0.005f;
        float m_zoom_speed = 0.01f;
        Vector3D m_center;
        QPointF m_last_mouse_pos;
        QPoint m_last_wheel_delta;
    };
};