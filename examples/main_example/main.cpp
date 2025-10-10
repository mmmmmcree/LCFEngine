#include "Application.h"
#include "RenderWindow.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "CameraControllers/TrackballController.h"
#include "Entity.h"
#include "TransformSystem.h"
#include "Timer.h"

class CustomRenderWindow : public lcf::RenderWindow
{
public:
    CustomRenderWindow(lcf::RenderWindow * parent = nullptr) : lcf::RenderWindow(parent)
    {
    }
protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
            case Qt::Key_F1: {
                this->setWindowState(Qt::WindowFullScreen);
            } break;
            case Qt::Key_Escape: {
                this->setWindowState(Qt::WindowNoState);
            } break;
        }
    }
};

int main(int argc, char* argv[]) {
    lcf::GuiApplication app(argc, argv);
    lcf::render::VulkanContext context;
    CustomRenderWindow render_window;
    context.registerWindow(&render_window);
    context.create();
    lcf::VulkanRenderer renderer(&context);
    renderer.setRenderTarget(render_window.getRenderTarget());
    renderer.create();
    render_window.resize(1280, 720);
    render_window.show();

    lcf::Registry registry;
    lcf::TransformSystem transform_system(&registry);
    
    lcf::Entity signal_manager(&registry);
    auto & transform_acquired_signal = signal_manager.requireComponent<lcf::TransformUpdateSignal>();
    transform_acquired_signal.connect<&lcf::TransformSystem::onTransformUpdate>(transform_system);
    auto & transform_hierarchy_attach_signal = signal_manager.requireComponent<lcf::TransformHierarchyAttachSignal>();
    transform_hierarchy_attach_signal.connect<&lcf::TransformSystem::onTransformHierarchyAttach>(transform_system);

    lcf::Entity camera_entity(&registry, &signal_manager);
    camera_entity.requireComponent<lcf::Transform>();
    
    lcf::modules::TrackballController trackball_controller;
    trackball_controller.setInputManager(render_window.getInputManager());

    int update_interval = 5;
    double refresh_rate = render_window.screen()->refreshRate();
    if (refresh_rate > 60.0) {
        update_interval /= refresh_rate / 60.0;
    }

    lcf::ThreadTimer timer;
    timer.setInterval(std::chrono::milliseconds(update_interval));
    timer.setCallback([&] {
        trackball_controller.update(camera_entity);
        transform_system.update();
        renderer.render(camera_entity);
    });
    timer.start();

    app.exec();

    return 0;
}