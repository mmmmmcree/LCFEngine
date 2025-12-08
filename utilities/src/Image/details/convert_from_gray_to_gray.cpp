#include "Image/details/convert_from_gray_to_gray.h"

void lcf::details::convert_from_gray_to_gray(Image::ImageVariant &src_image_var, Image::Format src_format, Image::Format dst_format)
{
    if (src_format == dst_format) { return; }
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
        case Image::Format::eGray8Uint: {
            src_image_var = convert_from_gray_to_gray<boost::gil::gray8_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eGray16Uint: {
            src_image_var = convert_from_gray_to_gray<boost::gil::gray16_image_t>(src_gray_image_var);
        } break;
        case Image::Format::eGray32Float: {
            src_image_var = convert_from_gray_to_gray<boost::gil::gray32f_image_t>(src_gray_image_var);
        } break;
    }
}