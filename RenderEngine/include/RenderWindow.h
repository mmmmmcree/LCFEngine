#pragma once

#include "Window.h"
#include "RHI/RenderTarget.h"
#include "Renderer.h"
#include <QScreen>

namespace lcf {
    class RenderWindow : public Window
    {
    public:
        RenderWindow(Window * parent = nullptr);
        ~RenderWindow() override;
        void show();
        void setRenderTarget(const render::RenderTarget::SharedPointer &render_target) { m_render_target = render_target; }
        const render::RenderTarget::SharedPointer & getRenderTarget() const { return m_render_target; }
    protected:
        void closeEvent(QCloseEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
    private:
        render::RenderTarget::SharedPointer  m_render_target;
    };
}