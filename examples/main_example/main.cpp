#include <QGuiApplication>
#include "RenderWindow.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "CameraControllers/TrackballController.h"
#include "Entity.h"
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    lcf::render::VulkanContext context;
    lcf::RenderWindow render_window;
    context.registerWindow(&render_window);
    context.create();
    lcf::VulkanRenderer renderer(&context);
    renderer.setRenderTarget(render_window.getRenderTarget());
    renderer.create();
    render_window.resize(1280, 720);
    render_window.show();

    lcf::Entity camera_entity;
    auto & camera_transform = camera_entity.requireComponent<lcf::Transform>();
    auto & projection_matrix = camera_entity.requireComponent<lcf::Matrix4x4>();
    projection_matrix.perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    lcf::InputManager * input_manager = render_window.getInputManager();
    lcf::modules::TrackballController trackball_controller;
    trackball_controller.setInputManager(input_manager);
    trackball_controller.controls(camera_transform);

    renderer.setCamera(camera_entity);


    QTimer render_timer;
    int update_interval = 5;
    double refresh_rate = render_window.screen()->refreshRate();
    if (refresh_rate > 60.0) {
        update_interval /= refresh_rate / 60.0;
    }
    render_timer.setInterval(update_interval);
    QObject::connect(&render_timer, &QTimer::timeout, [&] {
        trackball_controller.update();
        renderer.render();
    });
    render_timer.start();

    app.exec();
    return 0;
}