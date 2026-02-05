#pragma once

#include <boost/gil.hpp>
#include "float16.h"

namespace boost::gil {
    template <>
    struct channel_traits<lcf::float16_t>
    {
        using value_type = lcf::float16_t;
        using reference = lcf::float16_t &;
        using pointer = lcf::float16_t *;
        using const_reference = const lcf::float16_t &;
        using const_pointer = const lcf::float16_t *;
        static constexpr bool is_mutable = true;
        static value_type min_value() { return lcf::float16_t(0.0f); }
        static value_type max_value() { return lcf::float16_t(1.0f); }
    };

    template <>
    struct float_point_zero<lcf::float16_t>
    {
        static lcf::float16_t apply() { return lcf::float16_t(0.0f); }
    };

    template <>
    struct float_point_one<lcf::float16_t>
    {
        static lcf::float16_t apply() { return lcf::float16_t(1.0f); }
    };

    using float16_t = scoped_channel_value<lcf::float16_t, float_point_zero<lcf::float16_t>, float_point_one<lcf::float16_t>>;

    template <>
    struct channel_multiplier_unsigned<lcf::float16_t>
    {
        lcf::float16_t operator()(lcf::float16_t a, lcf::float16_t b) const { return a * b; }
    };

    template <>
    struct channel_multiplier_unsigned<float16_t>
    {
        float16_t operator()(float16_t a, float16_t b) const { return float16_t((lcf::float16_t(a)) * lcf::float16_t(b)); }
    };

    template <typename SrcChannel>
    struct channel_converter<SrcChannel, lcf::float16_t>
    {
        lcf::float16_t operator()(const SrcChannel& src) const { return lcf::float16_t(channel_convert<float>(src)); }
    };

    template <typename DstChannel>
    struct channel_converter<lcf::float16_t, DstChannel>
    {
        DstChannel operator()(const lcf::float16_t& src) const { return channel_convert<DstChannel>(static_cast<float>(src)); }
    };

    template <>
    struct channel_converter<lcf::float16_t, lcf::float16_t>
    {
        lcf::float16_t operator()(const lcf::float16_t& src) const { return src; }
    };

    template <typename SrcChannel>
    struct channel_converter<SrcChannel, float16_t>
    {
        float16_t operator()(const SrcChannel& src) const { return float16_t(lcf::float16_t(channel_convert<float>(src))); }
    };

    template <typename DstChannel>
    struct channel_converter<float16_t, DstChannel>
    {
        DstChannel operator()(const float16_t& src) const { return channel_convert<DstChannel>(static_cast<float>(lcf::float16_t(src))); }
    };

    template <>
    struct channel_converter<float16_t, float16_t>
    {
        float16_t operator()(const float16_t& src) const { return src; }
    };

    inline float operator*(const float16_t & src, float w)
    {
        return static_cast<float>(lcf::float16_t(src)) * w;
    }

    inline double operator*(const float16_t & src, double w)
    {
        return static_cast<double>(static_cast<float>(lcf::float16_t(src))) * w;
    }

    inline float operator*(float w, const float16_t & src) { return src * w; }

    inline double operator*(double w, const float16_t & src) { return src * w; }

namespace detail {
    struct cast_channel_convert_fn
    {
        template <typename SrcChannel, typename DstChannel>
        void operator()(const SrcChannel& src, DstChannel& dst) const
        {
            dst = channel_convert<typename channel_traits<DstChannel>::value_type>(src);
        }
    };
}

    template <typename SrcPixel, typename Layout>
    void cast_pixel(const SrcPixel& src, pixel<float16_t, Layout>& dst)
    {
        static_for_each(src, dst, detail::cast_channel_convert_fn{});
    }

    BOOST_GIL_DEFINE_BASE_TYPEDEFS(16f, float16_t, gray)
    BOOST_GIL_DEFINE_ALL_TYPEDEFS(16f, float16_t, gray_alpha)
    BOOST_GIL_DEFINE_ALL_TYPEDEFS(16f, float16_t, rgb)
    BOOST_GIL_DEFINE_ALL_TYPEDEFS(16f, float16_t, rgba)
}
