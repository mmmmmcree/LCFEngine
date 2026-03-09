#pragma once

#include "Registry.h"
#include "resources/ResourceHandle.h"
#include "resources/ResourceLoader.h"
#include "concepts/invocable_concept.h"

namespace lcf {
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
        ResourceHandle<Resource> load(Args &&... args) noexcept
        {
            using Loader = ResourceLoader<Resource, Args...>;
            if (not m_resource_registry.ctx().contains<Loader>()) { return {}; }
            auto && loader = m_resource_registry.ctx().get<Loader>();
            return loader.load(std::forward<Args>(args)...);
        }
        template <typename Resource>
        ResourceHandle<Resource> registerResource(Resource && resource) noexcept
        {
            ResourceHandle<Resource> handle {m_resource_registry};
            m_resource_registry.emplace<Resource>(handle.getArtifactID(), std::forward<Resource>(resource));
            auto & state = m_resource_registry.emplace<ResourceState>(handle.getArtifactID());
            state = ResourceState::eLoaded;
            return handle;
        }
    private:
        template <typename Resource, typename... Args>
        void registerLoader(ResourceLoader<Resource, Args...> loader)
        {
            using Loader = ResourceLoader<Resource, Args...>;
            m_resource_registry.ctx().emplace<Loader>(std::move(loader));
        }
    private:
        Registry * m_registry;
        Registry m_resource_registry;
    };
}
