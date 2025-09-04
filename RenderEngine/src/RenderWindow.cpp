#include "RenderWindow.h"
#include "VulkanRenderer.h"
#include <QDebug>

lcf::RenderWindow::RenderWindow(Window *parent) :
    Window(parent)
{
}

lcf::RenderWindow::~RenderWindow()
{
}

void lcf::RenderWindow::show()
{
    auto [x, y] = (this->screen()->size() - this->size()) / 2;
    this->setPosition(x, y);
    Window::show();
}

void lcf::RenderWindow::closeEvent(QCloseEvent *event)
{
    if (m_render_target) {
        m_render_target->destroy();
    }
    Window::closeEvent(event);
}

void lcf::RenderWindow::resizeEvent(QResizeEvent * event)
{
    if (m_render_target) {
        m_render_target->requireUpdate();
    }
    Window::resizeEvent(event);
}
