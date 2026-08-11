#pragma once

#include <vulkan/vulkan.hpp>
#include <utility>
#include "vk_core/pipeline/shader/ShaderObject.h"

namespace lcf::vkc {

class CommandBufferProxy;

class GraphicsPipelineInfo;

void bind_dynamic_graphics_state(
    CommandBufferProxy & cmd,
    const GraphicsPipelineInfo & info,
    const ShaderObjectBindingState & shader_object_binding_states
) noexcept;

} // namespace lcf::vkc
