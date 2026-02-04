#pragma once

#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include "gray_alpha_image_type.h"
#include "f16_image_types.h"

namespace lcf::details {
    using ImageVariant = boost::gil::any_image<
        boost::gil::gray8_image_t,
        boost::gil::gray16_image_t,
        boost::gil::gray16f_image_t,
        boost::gil::gray32f_image_t,

        boost::gil::gray_alpha8_image_t,
        boost::gil::gray_alpha16_image_t,
        boost::gil::gray_alpha16f_image_t,
        boost::gil::gray_alpha32f_image_t,
        
        boost::gil::rgb8_image_t,
        boost::gil::rgb16_image_t,
        boost::gil::rgb16f_image_t,
        boost::gil::rgb32f_image_t,
        
        boost::gil::rgba8_image_t,
        boost::gil::rgba16_image_t,
        boost::gil::rgba16f_image_t,
        boost::gil::rgba32f_image_t
    >;

    using ColorImageVariant = boost::gil::any_image<
        boost::gil::rgb8_image_t,
        boost::gil::rgb16_image_t,
        boost::gil::rgb16f_image_t,
        boost::gil::rgb32f_image_t,
        boost::gil::rgba8_image_t,
        boost::gil::rgba16_image_t,
        boost::gil::rgba16f_image_t,
        boost::gil::rgba32f_image_t
    >;

    using GrayImageVariant = boost::gil::any_image<
        boost::gil::gray8_image_t,
        boost::gil::gray16_image_t,
        boost::gil::gray16f_image_t,
        boost::gil::gray32f_image_t,
        boost::gil::gray_alpha8_image_t,
        boost::gil::gray_alpha16_image_t,
        boost::gil::gray_alpha16f_image_t,
        boost::gil::gray_alpha32f_image_t
    >;

    using ImageViewVariant = decltype(boost::gil::view(std::declval<ImageVariant &>()));

    using ConstImageViewVariant = decltype(boost::gil::const_view(std::declval<const ImageVariant &>()));

    using ColorImageViewVariant = decltype(boost::gil::view(std::declval<ColorImageVariant &>()));

    using ConstColorImageViewVariant = decltype(boost::gil::const_view(std::declval<const ColorImageVariant &>()));

    using GrayImageViewVariant = decltype(boost::gil::view(std::declval<GrayImageVariant &>()));

    using ConstGrayImageViewVariant = decltype(boost::gil::const_view(std::declval<const GrayImageVariant &>()));

}
