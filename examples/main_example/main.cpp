#include "gui_types.h"
#include "input_types.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "Timer.h"
#include "tasks/TaskScheduler.h"
#include "Transform.h"
#include "TransformSystem.h"
#include "CameraControllers/TrackballController.h"
#include "log.h"
#include "tracy_profiling.h"

using namespace std::chrono_literals;

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
    trackball_controller.setSensitivity(0.2f)
        .setMoveSpeed(50.0f)
        .setZoomSpeed(400.0f)
        .setInputReader(input_reader);

    auto render_loop = [&] {
        static auto last_time = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_time).count();
        last_time = now;

        input_reader.update(window_up->getComponent<lcf::InputCollector>().getSnapshot());
        trackball_controller.update(camera_entity, delta_time); //todo upadate multiple camera entity
        transform_system.update();
        renderer.render(camera_entity, window_up->getEntity()); //todo make a data pack
    };
        
    lcf::ThreadIOContext::SharedPointer thread_io_context_sp = lcf::ThreadIOContext::makeShared();
    thread_io_context_sp->run();
    lcf::Timer engine_timer(thread_io_context_sp);
    engine_timer.setCallback(std::move(render_loop))
        .setInterval(1ms)
        .start();

    lcf::TaskScheduler scheduler;
    lcf::PeriodicTask periodic_task(
        5ms,
        [&] {
            TRACY_SCOPE_BEGIN_NC("PollEvents", tracy::Color::Red2);
            window_up->pollEvents();
            TRACY_SCOPE_END();
        },
        [&] { return window_up->getState() != lcf::gui::WindowState::eAboutToClose; }
    );
    scheduler.registerPeriodicTask(std::move(periodic_task))
        .run();
    return 0;
}