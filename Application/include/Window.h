#pragma once

#include <QWindow>
#include "InputManager.h"

namespace lcf {
    class Window : public QWindow
    {
    public:
        Window(QWindow *parent = nullptr);
        Window(const Window &) = delete;
        Window(Window &&) = delete;
        Window & operator=(const Window &) = delete;
        Window & operator=(Window &&) = delete;
    };
}