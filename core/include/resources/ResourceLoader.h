#pragma once

#include "ResourceEntity.h"
#include "tasks/TaskScheduler.h"
#include "type_traits/callable_traits.h"
#include "concepts/invocable_concept.h"
#include <functional>
#include <optional>

namespace lcf {
    template <typename Resource, typename... Args>
    class ResourceLoader
    {
        using LoadFunc = std::function<std::optional<Resource>(Args...)>;
    public:
        ResourceLoader(ResourceRegistry & registry, LoadFunc && load_func) :
            m_registry_p(&registry),
            m_load_func(std::move(load_func))
        {}
        TypedResourceEntity<Resource> load(Args... args) const noexcept
        {
            auto opt_resource = m_load_func(std::move(args)...);
            if (not opt_resource) { return {}; }
            ResourceEntity res_entity {*m_registry_p};
            m_registry_p->emplace<Resource>(res_entity.getArtifactID(), std::move(opt_resource.value()));
            auto & state = m_registry_p->emplace<ResourceState>(res_entity.getArtifactID());
            state = ResourceState::eLoaded;
            return TypedResourceEntity<Resource>(res_entity);
        }
        // TypedResourceEntity<Resource> loadAsync(TaskScheduler & scheduler) const
        // {
        //     return {};
        // }
    private:
        ResourceRegistry * m_registry_p; // registry of RenderSystem
        LoadFunc m_load_func;
    };

namespace details {
    template <std::size_t... I>
    constexpr auto make_resource_loader_impl(ResourceRegistry & registry, auto && func, std::index_sequence<I...>)
    {
        using Traits = callable_traits<std::decay_t<decltype(func)>>;
        using Resource = typename Traits::result_type::value_type;
        using Args = typename Traits::arg_types;
        using Loader = ResourceLoader<Resource, std::decay_t<std::tuple_element_t<I, Args>>...>;
        return Loader{ registry, std::move(func) };
    }
}

    template <callable_c F>
    auto make_resource_loader(ResourceRegistry & registry, F && load_func)
    requires std::is_same_v<
        typename callable_traits<std::decay_t<F>>::result_type,
        std::optional<typename callable_traits<std::decay_t<F>>::result_type::value_type>>
    {
        using Args = typename callable_traits<std::decay_t<F>>::arg_types;
        return details::make_resource_loader_impl(registry, std::forward<F>(load_func), std::make_index_sequence<std::tuple_size_v<Args>>{});
    }
}
