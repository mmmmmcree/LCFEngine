#pragma once

#include "Image.h"

namespace lcf::details {
    void convert_from_color_to_gray(Image::ImageVariant & src_image_var, Image::Format src_format, Image::Format dst_format);
    
    template <typename DstImageType>
    Image::ImageVariant convert_from_color_to_gray(Image::ColorImageVariant & src_image_var);
}

template <typename DstImageType>
inline lcf::Image::ImageVariant lcf::details::convert_from_color_to_gray(Image::ColorImageVariant & src_image_var)
{
    auto dst_image = DstImageType(src_image_var.dimensions());
    boost::gil::copy_and_convert_pixels(boost::gil::view(src_image_var), boost::gil::view(dst_image));
    return Image::ImageVariant(std::move(dst_image));
}