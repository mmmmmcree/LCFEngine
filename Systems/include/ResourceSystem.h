#pragma once

#include "resources/ResourceRegistry.h"
#include "resources/ResourceLoader.h"
#include "concepts/invocable_concept.h"

namespace lcf::ecs {
    class ResourceSystem
    {
    public:
        ResourceSystem(Registry & registry);
        
        template <callable_c Loader>
        void registerLoader(Loader && loader)
        {
            return this->registerLoader(make_resource_loader(m_resource_registry, std::forward<Loader>(loader)));
        }
        template <typename Resource, typename... Args>
        TypedResourceEntity<Resource> load(Args &&... args) noexcept
        {
            using Loader = ResourceLoader<Resource, Args...>;
            if (not m_resource_registry.ctx().contains<Loader>()) { return {}; }
            auto && loader = m_resource_registry.ctx().get<Loader>();
            return loader.load(std::forward<Args>(args)...);
        }
        template <typename Resource>
        TypedResourceEntity<Resource> registerResource(Resource && resource) noexcept
        {
            ResourceEntity resource_entity {m_resource_registry};
            m_resource_registry.emplace<Resource>(resource_entity.getArtifactID(), std::forward<Resource>(resource));
            auto & state = m_resource_registry.emplace<ResourceState>(resource_entity.getArtifactID());
            state = ResourceState::eLoaded;
            return TypedResourceEntity<Resource>(resource_entity);
        }
    private:
        template <typename Resource, typename... Args>
        void registerLoader(ResourceLoader<Resource, Args...> loader)
        {
            using Loader = ResourceLoader<Resource, Args...>;
            m_resource_registry.ctx().emplace<Loader>(std::move(loader));
        }
    private:
        Registry * m_ecs_registry;
        ResourceRegistry m_resource_registry;
    };
}
