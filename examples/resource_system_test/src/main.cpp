#include "ecs/Dispatcher.h"
#include "ecs/Registry.h"
#include "log.h"
#include "image/Image.h"
#include "resources/ResourceLoader.h"
#include "resources/resources_signals.h"
#include "ResourceSystem.h"
#include "type_traits/callable_traits.h"

using namespace lcf;

struct  SomeResource
{
    SomeResource() = default;
    SomeResource(SomeResource && other) : a(std::exchange(other.a, 0)) {}
    ~SomeResource() noexcept
    {
        if (a) {
            lcf_log_info("Resource destroyed");
        }
    }
    int a = 1;
};

void on_resource_released(const ResourceReleasedSignal<SomeResource> & signal)
{
    lcf_log_info("Resource released: {}", 1);
}

int main()
{
    lcf::Logger::init();
    
    ecs::RegistryCreateInfo registry_info {};
    ecs::Registry registry {registry_info};

    auto & resource_system = registry.ctx().emplace<ecs::ResourceSystem>(registry);
    registry.ctx().get<ecs::Dispatcher>().connect<&on_resource_released>();

    resource_system.registerLoader([](const std::filesystem::path & path) -> std::optional<Image> {
        Image image;
        auto err_code = image.loadFromFile({path});
        if (err_code) {
            lcf_log_error("Failed to load image: {}", err_code.message());
            return std::nullopt;
        } else {
            lcf_log_info("Successfully loaded image");
        }
        return image;
    });
    auto img_handle = resource_system.load<Image>(std::filesystem::path("a.jpg"));
    auto img_cpy_handle = resource_system.registerResource(std::move(*img_handle));
    if (img_handle) {
        lcf_log_info("Image loaded: {}x{}", img_handle->getWidth(), img_handle->getHeight());
    }
    if (img_cpy_handle) {
        lcf_log_info("Image copied: {}x{}", img_cpy_handle->getWidth(), img_cpy_handle->getHeight());
    }

    resource_system.registerLoader([]() -> std::optional<SomeResource> {
        return SomeResource {};
    });
    lcf_log_info("666");
    ResourceLease lease;
    {
        auto resource_handle = resource_system.load<SomeResource>();
        // lease = resource_handle.getLease();
    }
    return 0;
}