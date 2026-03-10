#include "ResourceRegistry.h"

using namespace lcf;

ResourceRegistry::ResourceRegistry(ecs::Dispatcher & dispatcher)
{
    this->ctx().emplace<ecs::Dispatcher *>(&dispatcher);
}
