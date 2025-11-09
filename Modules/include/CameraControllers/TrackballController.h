#pragma once

#include "Transform.h"
#include "InputManager.h"
#include "Entity.h"
#include <optional>

namespace lcf::modules
{
    class TrackballController
    {
    public:
        TrackballController() = default;
        void setInputManager(const InputManager * input_manager) { m_input_manager = input_manager; }
        void update(Entity & camera);
    private:
        const InputManager * m_input_manager = nullptr;
        float m_sensitivity = 0.2f;
        float m_move_speed = 0.005f;
        float m_zoom_speed = 0.01f;
        Vector3D<float> m_center;
        Vector2D<double> m_last_mouse_pos;
        Vector2D<int> m_last_wheel_delta;
    };
};