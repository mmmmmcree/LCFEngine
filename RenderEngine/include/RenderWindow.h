#pragma once

#include "Window.h"
#include "RHI/Context.h"
#include "RHI/RenderTarget.h"
#include "Renderer.h"
#include <QScreen>

namespace lcf {
    class RenderWindow : public Window
    {
    public:
        RenderWindow(render::Context * context, Window * parent = nullptr);
        ~RenderWindow() override;
        void show();
        void setRenderTarget(render::RenderTarget * render_target) { m_render_target = render_target; }
        render::RenderTarget * getRenderTarget() const { return m_render_target; }
    protected:
        void closeEvent(QCloseEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
    private:
        render::Context * m_context = nullptr;
        render::RenderTarget * m_render_target = nullptr;
    };
}