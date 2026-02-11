#pragma once

#include "shader_core_fwd_decls.h"
#include "JSON.h"

namespace lcf {
    OrderedJSON serialize(const ShaderResourceMember & shader_resource_member);

    OrderedJSON serialize(const ShaderResource & shader_resource);

    OrderedJSON serialize(const ShaderResources & shader_resources);
}