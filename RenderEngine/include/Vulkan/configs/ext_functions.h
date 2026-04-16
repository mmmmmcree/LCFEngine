#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render::vkext {

    //! Load extension function pointers that are not part of the core Vulkan spec.
    //! Under dynamic dispatch this is a no-op (the dispatcher handles it).
    //! Under static dispatch this calls getProcAddr for each known extension function.
    void load_instance_extensions(vk::Instance instance);

} // namespace lcf::render::vkext
