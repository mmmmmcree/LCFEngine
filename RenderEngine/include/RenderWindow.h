#pragma once

#include "Window.h"
#include "common/RenderTarget.h"

namespace lcf::render {
    class RenderWindow : public Window
    {
    public:
        RenderWindow(Window * parent = nullptr);
        RenderWindow(const RenderWindow &) = delete;
        RenderWindow & operator=(const RenderWindow &) = delete;
        ~RenderWindow() override;
        void show();
        void setRenderTarget(const RenderTarget::SharedPointer &render_target);
        RenderTarget::WeakPointer getRenderTarget() const { return m_render_target; }
    protected:
        bool event(QEvent *event) override;
        void closeEvent(QCloseEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
    private:
        RenderTarget::SharedPointer  m_render_target;
    };
}