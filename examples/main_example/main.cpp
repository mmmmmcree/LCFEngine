#include <QGuiApplication>
#include "RenderWindow.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    for (auto screen : QGuiApplication::screens()) {
        qDebug() << "Screen" << screen->name() << " " << screen->size() * screen->devicePixelRatio() << screen->availableSize() << screen->availableVirtualSize();
    }

    lcf::render::VulkanContext context;
    lcf::RenderWindow render_window(&context);
    context.create();
    lcf::VulkanRenderer renderer(&context);
    renderer.setRenderTarget(render_window.getRenderTarget());
    renderer.create();
    render_window.resize(1280, 720);
    render_window.show();

    QTimer render_timer;
    int update_interval = 5;
    double refresh_rate = render_window.screen()->refreshRate();
    if (refresh_rate > 60.0) {
        update_interval /= refresh_rate / 60.0;
    }
    render_timer.setInterval(update_interval);
    QObject::connect(&render_timer, &QTimer::timeout, [&renderer]() {
        renderer.render();
    });
    render_timer.start();

    app.exec();
    return 0;
}