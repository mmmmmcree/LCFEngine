#include "RenderWindow.h"
#include "VulkanRenderer.h"
#include <QScreen>
#include <QApplication>

using namespace lcf::render;

RenderWindow::RenderWindow(Window *parent) :
    Window(parent)
{
}

RenderWindow::~RenderWindow()
{
}


void RenderWindow::setRenderTarget(const render::RenderTarget::SharedPointer &render_target)
{
    m_render_target = render_target;
    QScreen *screen = this->screen();
    auto [maximal_width, maximal_height] = screen->devicePixelRatio() * screen->size();
    m_render_target->setMaximalExtent(maximal_width, maximal_height);
}

void RenderWindow::show()
{
    auto [x, y] = (this->screen()->size() - this->size()) / 2;
    this->setPosition(x, y);
    Window::show();
}

bool RenderWindow::event(QEvent * event)
{
    switch(event->type()) {
        case QEvent::Resize: {

        } return true;
        case QEvent::PlatformSurface: {
            auto surface_event = static_cast<QPlatformSurfaceEvent *>(event);
            if (surface_event->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                m_render_target->setSelient(); //- 1. notify render target about surface destruction
                while (m_render_target.use_count() > 1); // - 2. wait for all rendering tasks to finish
            }
        }
    }
    return Window::event(event);
}

void RenderWindow::closeEvent(QCloseEvent *event)
{
    Window::closeEvent(event);
}

void RenderWindow::resizeEvent(QResizeEvent *event)
{
    // Window::resizeEvent(event);
    
}
