#pragma once

#include "Entity.h"
#include "input_fwd_decls.h"
#include "Vector.h"

namespace lcf::modules {
    class TrackballController
    {
        using Self = TrackballController;
    public:
        TrackballController() = default;
        void setInputReader(const InputReader & input_reader) { m_input_reader = &input_reader; }
        void update(Entity & camera, float delta_time);
        Self & setSensitivity(float sensitivity) noexcept { m_sensitivity = sensitivity; return *this; }
        Self & setMoveSpeed(float move_speed) noexcept { m_move_speed = move_speed; return *this; }
        Self & setZoomSpeed(float zoom_speed) noexcept { m_zoom_speed = zoom_speed; return *this; }
    private:
        const InputReader * m_input_reader = nullptr;
        float m_sensitivity = 80.0f;
        float m_move_speed = 50.0f;
        float m_zoom_speed = 400.0f;
        Vector3D<float> m_center;
    };
};