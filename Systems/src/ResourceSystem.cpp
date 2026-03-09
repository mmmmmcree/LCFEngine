#include "ResourceSystem.h"
#include "Dispatcher.h"

using namespace lcf;

ResourceSystem::ResourceSystem(Registry & registry) :
    m_registry(&registry),
    m_resource_registry(RegistryCreateInfo {})
{
    m_resource_registry.ctx().emplace<Dispatcher *>(&m_registry->ctx().get<Dispatcher>());
    //todo use entt::registry instead of Registry?
}