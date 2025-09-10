#include "Window.h"

lcf::Window::Window(QWindow *parent) :
    QWindow(parent),
    m_input_manager(new lcf::InputManager(this))
{
}