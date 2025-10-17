#include "Window.h"

lcf::Window::Window(QWindow *parent) :
    QWindow(parent)
{
    //! 自定义bool event(QEvent *event),不依赖Qwindow的默认
}