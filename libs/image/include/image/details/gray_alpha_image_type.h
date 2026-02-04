#pragma once
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/gray_alpha.hpp>
#include <boost/gil/color_convert.hpp>

namespace boost::gil {
    template <typename SrcChannel, typename DstChannel>
    inline void color_convert(const pixel<SrcChannel, gray_layout_t>& src, pixel<DstChannel, gray_alpha_layout_t>& dst)
    {
        get_color(dst, gray_color_t()) = channel_convert<DstChannel>(get_color(src, gray_color_t()));
        get_color(dst, alpha_t()) = channel_traits<DstChannel>::max_value();
    }

    template <typename SrcChannel, typename DstChannel>
    inline void color_convert(const pixel<SrcChannel, gray_alpha_layout_t>& src, pixel<DstChannel, gray_layout_t>& dst)
    {
        get_color(dst, gray_color_t()) = channel_convert<DstChannel>(get_color(src, gray_color_t()));
    }

    template <>
    struct default_color_converter_impl<gray_t, gray_alpha_t>
    {
        template <typename SrcP, typename DstP>
        void operator()(const SrcP& src, DstP& dst) const
        {
            using DstChannel = typename channel_type<DstP>::type;
            get_color(dst, gray_color_t()) = channel_convert<DstChannel>(get_color(src, gray_color_t()));
            get_color(dst, alpha_t()) = channel_traits<DstChannel>::max_value();
        }
    };

    template <>
    struct default_color_converter_impl<rgb_t, gray_alpha_t>
    {
        template <typename SrcP, typename DstP>
        void operator()(const SrcP& src, DstP& dst) const
        {
            using DstChannel = typename channel_type<DstP>::type;
            const float r = channel_convert<float>(get_color(src, red_t()));
            const float g = channel_convert<float>(get_color(src, green_t()));
            const float b = channel_convert<float>(get_color(src, blue_t()));
            const float y = 0.299f * r + 0.587f * g + 0.114f * b;
            get_color(dst, gray_color_t()) = channel_convert<DstChannel>(y);
            get_color(dst, alpha_t()) = channel_traits<DstChannel>::max_value();
        }
    };

    template <>
    struct default_color_converter_impl<rgba_t, gray_alpha_t>
    {
        template <typename SrcP, typename DstP>
        void operator()(const SrcP& src, DstP& dst) const
        {
            using DstChannel = typename channel_type<DstP>::type;
            const float r = channel_convert<float>(get_color(src, red_t()));
            const float g = channel_convert<float>(get_color(src, green_t()));
            const float b = channel_convert<float>(get_color(src, blue_t()));
            const float y = 0.299f * r + 0.587f * g + 0.114f * b;
            get_color(dst, gray_color_t()) = channel_convert<DstChannel>(y);
            get_color(dst, alpha_t()) = channel_convert<DstChannel>(get_color(src, alpha_t()));
        }
    };
}
