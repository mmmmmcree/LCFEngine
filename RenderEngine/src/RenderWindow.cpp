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

bool lcf::RenderWindow::event(QEvent * event)
{
    switch(event->type()) {
        case QEvent::PlatformSurface: {
            auto surface_event = static_cast<QPlatformSurfaceEvent *>(event);
            if (surface_event->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                qDebug() << "SurfaceAboutToBeDestroyed";
                /*
                todo 1. notify render target about surface destruction, may need atomic bool flag in render target
                todo 2. wait for all rendering tasks to finish, may need to wait for a semaphore or fence
                todo 3. destroy surface and render target
                */
            }
        }
    }
    return Window::event(event);
}

void lcf::RenderWindow::closeEvent(QCloseEvent *event)
{
    // while (m_render_target.use_count() > 1);
    Window::closeEvent(event);
}

void lcf::RenderWindow::resizeEvent(QResizeEvent *event)
{
    Window::resizeEvent(event);
}
