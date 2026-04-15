#include "ecs/Dispatcher.h"
#include "ecs/Registry.h"
#include "log.h"
#include "image/Image.h"
#include "resources/ResourceRegistry.h"
#include "resources/ResourceEntity.h"
#include "resources/ResourceLoader.h"
#include "resources/resources_signals.h"
#include "ResourceSystem.h"

using namespace lcf;

struct SomeResource
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

void on_resource_released(const ResourceReleasedSignal & signal)
{
    lcf_log_info("Resource released: {}", static_cast<uint32_t>(signal.m_artifact_id));
}

int main()
{
    lcf::Logger::init();
    
    ecs::RegistryCreateInfo registry_info {};
    ecs::Registry registry {registry_info};

    auto & resource_system = registry.ctx().emplace<ecs::ResourceSystem>(registry);
    registry.ctx().get<ecs::Dispatcher>().connect<&on_resource_released>();

    auto load_image = [](const std::filesystem::path & path) -> std::optional<Image> {
        Image image;
        auto err_code = image.loadFromFile({path});
        if (err_code) {
            lcf_log_error("Failed to load image: {}", err_code.message());
            return std::nullopt;
        } else {
            lcf_log_info("Successfully loaded image");
        }
        return image;
    };

    resource_system.registerLoader(std::move(load_image));
    auto img_res_entity = resource_system.load<Image>(std::filesystem::path("assets/images/bk.jpg"));
    auto img_cpy_res_entity = resource_system.registerResource(std::move(*img_res_entity));
    if (img_res_entity) {
        lcf_log_info("Image loaded: {}x{}", img_res_entity->getWidth(), img_res_entity->getHeight());
    }
    if (img_cpy_res_entity) {
        lcf_log_info("Image copied: {}x{}", img_cpy_res_entity->getWidth(), img_cpy_res_entity->getHeight());
    }
    resource_system.registerLoader([]() -> std::optional<SomeResource> {
        return SomeResource {};
    });
    lcf_log_info("666");
    ResourceLease lease;
    {
        auto res_entity = resource_system.load<SomeResource>();
        lease = res_entity.lease();
    }
    return 0;
}