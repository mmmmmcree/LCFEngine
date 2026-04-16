#pragma once

#include <vulkan/vulkan.hpp>
#include <span>

namespace lcf::render::vkreq {

    //! Instance extensions required by the engine (excluding windowing system extensions).
    std::span<const char * const> get_instance_extensions() noexcept;

    //! Instance layers required by the engine.
    std::span<const char * const> get_instance_layers() noexcept;

    //! Device extensions required by the engine (excluding presentation extensions).
    std::span<const char * const> get_device_extensions() noexcept;

    //! Device extensions required for presentation (swapchain etc.).
    std::span<const char * const> get_presentation_device_extensions() noexcept;

} // namespace lcf::render::vkreq
