#pragma once

#include "Registry.h"
#include "core/resources/ResourceHandle.h"
#include "core/resources/ResourceLoader.h"
#include "concepts/invocable_concept.h"

namespace lcf {
    class ResourceSystem
    {
    public:
        ResourceSystem(Registry & registry) : m_registry(&registry), m_resource_registry({}) {}
        template <typename Resource, typename... Args>
        void registerLoader(ResourceLoader<Resource, Args...> loader)
        {
            using Loader = ResourceLoader<Resource, Args...>;
            m_resource_registry.ctx().emplace<Loader>(std::move(loader));
        }
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
    private:
        Registry * m_registry;
        Registry m_resource_registry;
    };
}
