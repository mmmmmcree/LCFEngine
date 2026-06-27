#include "win/error.h"

namespace {

using namespace lcf::win;

class ErrorCategory : public std::error_category
{
public:
    const char * name() const noexcept override { return "lcf::win"; }

    std::string message(int value) const override
    {
        switch (static_cast<errc>(value)) {
            case errc::no_error:
                return "no error";
            case errc::backend_init_failed:
                return "window backend failed to initialize";
            case errc::window_creation_failed:
                return "the window could not be created";
            case errc::display_query_failed:
                return "the primary display could not be queried";
            case errc::native_handle_unavailable:
                return "the platform native window handle is unavailable";
            case errc::metal_layer_creation_failed:
                return "the CAMetalLayer could not be created";
            default:
                return "unrecognized lcf::win error";
        }
    }
};

const ErrorCategory & error_category() noexcept
{
    static const ErrorCategory instance;
    return instance;
}

} // namespace

namespace lcf::win {

std::error_code make_error_code(errc error) noexcept
{
    return { static_cast<int>(error), error_category() };
}

} // namespace lcf::win
