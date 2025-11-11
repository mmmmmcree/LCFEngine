#include "WindowSystem.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "Timer.h"
#include "Transform.h"

int main(int argc, char *argv[])
{
    auto window_up = lcf::gui::WindowSystem::getInstance().allocateWindow();
    lcf::render::VulkanContext context;
    context.registerWindow(window_up.get())
        .create();

    lcf::VulkanRenderer renderer;
    renderer.create(&context, {2560, 1440});
    window_up->show();

    lcf::Registry registry;
    lcf::Entity camera_entity(&registry);
    camera_entity.requireComponent<lcf::Transform>();

    auto render_loop = [&] {
        renderer.render(camera_entity, context.m_surface_render_targets[0]);
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