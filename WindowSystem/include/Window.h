#pragma once

#include <QWindow>

namespace lcf {
    class Window : public QWindow
    {
    public:
        Window(QWindow *parent = nullptr);
    };
}