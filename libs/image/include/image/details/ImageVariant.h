#pragma once

#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include "gray_alpha_image_type.h"
#include "f16_image_types.h"

namespace lcf::details {
    template <typename Allocator = std::allocator<unsigned char>>
    using gray8_image_t = boost::gil::image<boost::gil::gray8_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray16_image_t = boost::gil::image<boost::gil::gray16_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray16f_image_t = boost::gil::image<boost::gil::gray16f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray32f_image_t = boost::gil::image<boost::gil::gray32f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray_alpha8_image_t = boost::gil::image<boost::gil::gray_alpha8_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray_alpha16_image_t = boost::gil::image<boost::gil::gray_alpha16_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray_alpha16f_image_t = boost::gil::image<boost::gil::gray_alpha16f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using gray_alpha32f_image_t = boost::gil::image<boost::gil::gray_alpha32f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgb8_image_t = boost::gil::image<boost::gil::rgb8_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgb16_image_t = boost::gil::image<boost::gil::rgb16_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgb16f_image_t = boost::gil::image<boost::gil::rgb16f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgb32f_image_t = boost::gil::image<boost::gil::rgb32f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgba8_image_t = boost::gil::image<boost::gil::rgba8_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgba16_image_t = boost::gil::image<boost::gil::rgba16_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgba16f_image_t = boost::gil::image<boost::gil::rgba16f_pixel_t, false, Allocator>;
    template <typename Allocator = std::allocator<unsigned char>>
    using rgba32f_image_t = boost::gil::image<boost::gil::rgba32f_pixel_t, false, Allocator>;

    template <typename Allocator = std::allocator<unsigned char>>
    using ImageVariant = boost::gil::any_image<
        gray8_image_t<Allocator>,
        gray16_image_t<Allocator>,
        gray16f_image_t<Allocator>,
        gray32f_image_t<Allocator>,

        gray_alpha8_image_t<Allocator>,
        gray_alpha16_image_t<Allocator>,
        gray_alpha16f_image_t<Allocator>,
        gray_alpha32f_image_t<Allocator>,

        rgb8_image_t<Allocator>,
        rgb16_image_t<Allocator>,
        rgb16f_image_t<Allocator>,
        rgb32f_image_t<Allocator>,

        rgba8_image_t<Allocator>,
        rgba16_image_t<Allocator>,
        rgba16f_image_t<Allocator>,
        rgba32f_image_t<Allocator>
    >;

    template <typename Allocator = std::allocator<unsigned char>>
    using ColorImageVariant = boost::gil::any_image<
        rgb8_image_t<Allocator>,
        rgb16_image_t<Allocator>,
        rgb16f_image_t<Allocator>,
        rgb32f_image_t<Allocator>,
        rgba8_image_t<Allocator>,
        rgba16_image_t<Allocator>,
        rgba16f_image_t<Allocator>,
        rgba32f_image_t<Allocator>
    >;

    template <typename Allocator = std::allocator<unsigned char>>
    using GrayImageVariant = boost::gil::any_image<
        gray8_image_t<Allocator>,
        gray16_image_t<Allocator>,
        gray16f_image_t<Allocator>,
        gray32f_image_t<Allocator>,
        gray_alpha8_image_t<Allocator>,
        gray_alpha16_image_t<Allocator>,
        gray_alpha16f_image_t<Allocator>,
        gray_alpha32f_image_t<Allocator>
    >;

    using ImageViewVariant = decltype(boost::gil::view(std::declval<ImageVariant<> &>()));

    using ConstImageViewVariant = decltype(boost::gil::const_view(std::declval<const ImageVariant<> &>()));

    using ColorImageViewVariant = decltype(boost::gil::view(std::declval<ColorImageVariant<> &>()));

    using ConstColorImageViewVariant = decltype(boost::gil::const_view(std::declval<const ColorImageVariant<> &>()));

    using GrayImageViewVariant = decltype(boost::gil::view(std::declval<GrayImageVariant<> &>()));

    using ConstGrayImageViewVariant = decltype(boost::gil::const_view(std::declval<const GrayImageVariant<> &>()));
}
