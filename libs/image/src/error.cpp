#include "image/error.h"

#include <string>

namespace {

class ImageErrorCategory final : public std::error_category
{
public:
    const char * name() const noexcept override;
    std::string message(int value) const override;
};

const ImageErrorCategory & image_error_category() noexcept;

} // namespace

namespace lcf::img {

std::error_code make_error_code(errc error) noexcept
{
    return {static_cast<int>(error), image_error_category()};
}

} // namespace lcf::img

namespace {

const char * ImageErrorCategory::name() const noexcept
{
    return "lcf::img";
}

std::string ImageErrorCategory::message(int value) const
{
    switch (static_cast<lcf::img::errc>(value)) {
        case lcf::img::errc::no_error: return "no error";
        case lcf::img::errc::invalid_file_type: return "the image file type is not supported";
        case lcf::img::errc::file_not_found: return "the image file was not found";
        case lcf::img::errc::invalid_image: return "the image file is invalid or cannot be decoded";
        case lcf::img::errc::unsupported_format: return "the image format is not supported";
        default: return "unrecognized lcf::img error";
    }
}

const ImageErrorCategory & image_error_category() noexcept
{
    static const ImageErrorCategory instance;
    return instance;
}

} // namespace
