#include "ResourceSystem.h"
#include "ecs/Registry.h"
#include "ecs/Dispatcher.h"

using namespace lcf::ecs;

ResourceSystem::ResourceSystem(Registry & registry) :
    m_ecs_registry(&registry),
    m_resource_registry(registry.ctx().get<Dispatcher>())
{
}