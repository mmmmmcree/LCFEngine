#include "Registry.h"
#include "log.h"
#include "image/Image.h"
#include "core/resources/ResourceLoader.h"
#include "ResourceSystem.h"

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

int main()
{
    lcf::Logger::init();
    
    RegistryCreateInfo registry_info {};
    Registry registry {registry_info};

    ResourceSystem resource_system {registry};

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
    if (img_handle) {
        lcf_log_info("Image loaded: {}x{}", img_handle->getWidth(), img_handle->getHeight());
    }

    resource_system.registerLoader([]() -> std::optional<SomeResource> {
        return SomeResource {};
    });
    lcf_log_info("666");
    ResourceLeash leash;
    {
        auto resource_handle = resource_system.load<SomeResource>();
        leash = resource_handle.getLeash();
    }
    lcf_log_info("666");

    return 0;
}