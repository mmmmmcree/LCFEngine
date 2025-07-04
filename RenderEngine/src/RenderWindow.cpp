#include "RenderWindow.h"
#include "VulkanRenderer.h"
#include <QDebug>

lcf::RenderWindow::RenderWindow(render::Context *context, Window *parent) :
    Window(parent), 
    m_context(context)
{
    if (context->isCreated()) {
        qDebug() << "RenderWindow should be constructed before the context is created";
        return;
    }
    context->registerWindow(this);
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
    m_context->unregisterWindow(this);
    Window::closeEvent(event);
}

void lcf::RenderWindow::resizeEvent(QResizeEvent * event)
{
    if (m_render_target) {
        m_render_target->requireUpdate();
    }
    Window::resizeEvent(event);
}
