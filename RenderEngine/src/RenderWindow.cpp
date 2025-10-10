#include "RenderWindow.h"
#include "VulkanRenderer.h"

using namespace lcf;

lcf::RenderWindow::RenderWindow(Window *parent) :
    Window(parent)
{
}

lcf::RenderWindow::~RenderWindow()
{
}


void RenderWindow::setRenderTarget(const render::RenderTarget::SharedPointer &render_target)
{
    m_render_target = render_target;
    QScreen *screen = this->screen();
    auto [maximal_width, maximal_height] = screen->devicePixelRatio() * screen->size();
    m_render_target->setMaximalExtent(maximal_width, maximal_height);
}

void lcf::RenderWindow::show()
{
    auto [x, y] = (this->screen()->size() - this->size()) / 2;
    this->setPosition(x, y);
    Window::show();
}

void lcf::RenderWindow::resizeEvent(QResizeEvent * event)
{
    if (m_render_target) {
        m_render_target->requireUpdate();
    }
    Window::resizeEvent(event);
}
