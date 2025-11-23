#include "gui_types.h"
#include "input_types.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "Timer.h"
#include "Transform.h"
#include "TransformSystem.h"
#include "CameraControllers/TrackballController.h"
#include "log.h"

int main(int argc, char *argv[])
{
    lcf::Logger::init();
    lcf::Registry registry;
    lcf::render::VulkanContext context; //- create context before create vulkan window(load vulkan if use dynamic link library)

    auto window_up = lcf::gui::WindowSystem::getInstance().allocateWindow();
    lcf::gui::WindowCreateInfo window_info;
    window_info.setRegistryPtr(&registry)
        .setWidth(1280)
        .setHeight(720)
        .setSurfaceType(lcf::gui::SurfaceType::eVulkan);
    window_up->create(window_info); //- create window before register to context
    window_up->show(); //- show after create


    context.registerWindow(window_up->getEntity())
        .create();

    lcf::VulkanRenderer renderer;
    renderer.create(&context, lcf::gui::WindowSystem::getInstance().getPrimaryDisplayerInfo().getDesktopModeInfo().getRenderExtent());

    lcf::TransformSystem transform_system(registry);
    lcf::Entity camera_entity(registry);
    auto & transform = camera_entity.requireComponent<lcf::Transform>();
    camera_entity.requireComponent<lcf::TransformInvertedWorldMatrix>(transform.getWorldMatrix());

    lcf::InputReader input_reader;
    lcf::modules::TrackballController trackball_controller;
    trackball_controller.setInputReader(input_reader);

    auto render_loop = [&] {
        input_reader.update(window_up->getComponent<lcf::InputCollector>().getSnapshot());
        trackball_controller.update(camera_entity); //todo upadate multiple camera entity
        transform_system.update();
        renderer.render(camera_entity, window_up->getEntity()); //todo make a data pack
    };
        
    lcf::ThreadTimer timer;
    timer.setCallback(render_loop)
        .setInterval(std::chrono::milliseconds(1))
        .start();

    while (true) {
        window_up->pollEvents();
        if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { break; }
    }
    return 0;
}