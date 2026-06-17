#include "vk_core/error.h"

namespace {

using namespace lcf::vkc;

class ErrorCategory : public std::error_category
{
public:
    const char * name() const noexcept override { return "lcf::vkc"; }

    std::string message(int value) const override
    {
        switch (static_cast<errc>(value)) {
            case errc::no_error: 
                return "no error";
            case errc::no_suitable_instance:
                return "no instance satisfies the selection requirements";
            case errc::no_suitable_physical_device:
                return "no physical device satisfies the selection requirements";
            case errc::no_suitable_queue_family:
                return "no queue family provides the required capabilities";
            case errc::preferred_device_type_unavailable:
                return "no physical device of the preferred type is available";
            case errc::no_suitable_surface_format:
                return "surface exposes no supported format";
            case errc::no_suitable_present_mode:
                return "surface exposes no supported present mode";
            case errc::no_suitable_present_queue_family:
                return "no queue family supports presentation to the surface";
            case errc::main_device_role_not_configured:
                return "the main device role must be configured";
            default:
                return "unrecognized lcf::vkc error";
        }
    }
};

const ErrorCategory & error_category() noexcept
{
    static const ErrorCategory instance;
    return instance;
}

} // namespace

namespace lcf::vkc {

std::error_code make_error_code(errc error) noexcept
{
    return { static_cast<int>(error), error_category() };
}

} // namespace lcf::vkc
