#pragma once

#include "Entity.h"
#include "input_forward_declares.h"
#include "Vector.h"

namespace lcf::modules {
    class TrackballController
    {
    public:
        TrackballController() = default;
        void setInputReader(const InputReader & input_reader) { m_input_reader = &input_reader; }
        void update(Entity & camera);
    private:
        const InputReader * m_input_reader = nullptr;
        float m_sensitivity = 0.2f;
        float m_move_speed = 0.05f;
        float m_zoom_speed = 1.0f;
        Vector3D<float> m_center;
    };
};