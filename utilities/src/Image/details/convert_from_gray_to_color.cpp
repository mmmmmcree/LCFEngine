#include "Image/details/convert_from_gray_to_color.h"

void lcf::details::convert_from_gray_to_color(Image::ImageVariant &src_image_var, Image::Format src_format, Image::Format dst_format)
{
    Image::GrayImageVariant src_gray_image_var;
    switch (src_format) {
        case Image::Format::eGray8Uint: {
            src_gray_image_var = std::move(boost::variant2::get<boost::gil::gray8_image_t>(src_image_var));
        } break;
        case Image::Format::eGray16Uint: {
            src_gray_image_var = std::move(boost::variant2::get<boost::gil::gray16_image_t>(src_image_var));
        } break;
        case Image::Format::eGray32Float: {
            src_gray_image_var = std::move(boost::variant2::get<boost::gil::gray32f_image_t>(src_image_var));
        } break;
    }
    switch (dst_format) {
        case Image::Format::eRGB8Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::rgb8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eRGB16Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::rgb16_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eRGB32Float: {
            src_image_var = convert_from_gray_to_color<boost::gil::rgb32f_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eRGBA8Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::rgba8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eRGBA16Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::rgba16_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eRGBA32Float: {
            src_image_var = convert_from_gray_to_color<boost::gil::rgba32f_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eBGR8Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::bgr8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eBGRA8Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::bgra8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eARGB8Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::argb8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eCMYK8Uint: {
            src_image_var = convert_from_gray_to_color<boost::gil::cmyk8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eCMYK32Float: {
            src_image_var = convert_from_gray_to_color<boost::gil::cmyk32f_image_t>(src_gray_image_var);
        } break;
    }
}