#include "convert_from_color_to_color.h"

void lcf::details::convert_from_color_to_color(Image::ImageVariant &src_image_var, Image::Format src_format, Image::Format dst_format)
{
    if (src_format == dst_format) { return; }
    Image::ColorImageVariant src_color_image_var;
    switch (src_format) {
        case Image::Format::eRGB8Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::rgb8_image_t>(src_image_var));
        } break;
        case Image::Format::eRGB16Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::rgb16_image_t>(src_image_var));
        } break;
        case Image::Format::eRGB32Float: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::rgb32f_image_t>(src_image_var));
        } break;
        case Image::Format::eRGBA8Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::rgba8_image_t>(src_image_var));
        } break;
        case Image::Format::eRGBA16Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::rgba16_image_t>(src_image_var));
        } break;
        case Image::Format::eRGBA32Float: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::rgba32f_image_t>(src_image_var));
        } break;
        case Image::Format::eBGR8Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::bgr8_image_t>(src_image_var));
        } break;
        case Image::Format::eBGRA8Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::bgra8_image_t>(src_image_var));
        } break;
        case Image::Format::eARGB8Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::argb8_image_t>(src_image_var));
        } break;
        case Image::Format::eCMYK8Uint: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::cmyk8_image_t>(src_image_var));
        } break;
        case Image::Format::eCMYK32Float: {
            src_color_image_var = std::move(boost::variant2::get<boost::gil::cmyk32f_image_t>(src_image_var));
        } break;
    }
    switch (dst_format) {
        case Image::Format::eRGB8Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::rgb8_image_t>(src_color_image_var);
        } break;
        case Image::Format::eRGB16Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::rgb16_image_t>(src_color_image_var);
        } break;
        case Image::Format::eRGB32Float: {
            src_image_var = convert_from_color_to_color<boost::gil::rgb32f_image_t>(src_color_image_var);
        } break;
        case Image::Format::eRGBA8Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::rgba8_image_t>(src_color_image_var);
        } break;
        case Image::Format::eRGBA16Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::rgba16_image_t>(src_color_image_var);
        } break;
        case Image::Format::eRGBA32Float: {
            src_image_var = convert_from_color_to_color<boost::gil::rgba32f_image_t>(src_color_image_var);
        } break;
        case Image::Format::eBGR8Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::bgr8_image_t>(src_color_image_var);
        } break;
        case Image::Format::eBGRA8Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::bgra8_image_t>(src_color_image_var);
        } break;
        case Image::Format::eARGB8Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::argb8_image_t>(src_color_image_var);
        } break;
        case Image::Format::eCMYK8Uint: {
            src_image_var = convert_from_color_to_color<boost::gil::cmyk8_image_t>(src_color_image_var);
        } break;
        case Image::Format::eCMYK32Float: {
            src_image_var = convert_from_color_to_color<boost::gil::cmyk32f_image_t>(src_color_image_var);
        } break;
    }
}