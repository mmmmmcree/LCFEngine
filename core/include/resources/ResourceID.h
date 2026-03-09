#pragma once

#include <boost/uuid.hpp>
#include <entt/entity/fwd.hpp>

namespace lcf {
    using ResourceArtifactID = entt::entity;
    using ResourceUUID = boost::uuids::uuid;
}