#pragma once

#include <QWindow>
#include "InputManager.h"

namespace lcf {
    class Window : public QWindow
    {
    public:
        Window(QWindow *parent = nullptr);
        InputManager * getInputManager() const { return m_input_manager; }
    protected:
        InputManager * m_input_manager;
    };
}