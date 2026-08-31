#pragma once

#include "image_enums.h"
#include <cstddef>

namespace lcf::img {

template <typename Enum>
struct enum_traits;

template <>
struct enum_traits<PixelDataType>
{
    static constexpr uint8_t get_bytes_per_channel(PixelDataType type) noexcept
    {
        switch (type) {
            case PixelDataType::eUint8: return 1;
            case PixelDataType::eUint16:
            case PixelDataType::eFloat16: return 2;
            case PixelDataType::eUint32:
            case PixelDataType::eFloat32: return 4;
            default: return 0;
        }
    }

    static constexpr bool is_supported(PixelDataType type) noexcept
    {
        return type == PixelDataType::eUint8 or type == PixelDataType::eUint16 or
            type == PixelDataType::eFloat16 or type == PixelDataType::eFloat32;
    }
};

template <>
struct enum_traits<ColorSpace>
{
    static constexpr uint8_t get_channel_count(ColorSpace color_space) noexcept
    {
        if (color_space == ColorSpace::eInvalid) { return 0; }
        return (std::to_underlying(color_space) & 0x3) + 1;
    }

    static constexpr bool is_color_format(ColorSpace color_space) noexcept
    {
        return color_space != ColorSpace::eInvalid and ((std::to_underlying(color_space) >> 2) & 1) != 0;
    }

    static constexpr ColorSpace decode(uint8_t channel_count) noexcept
    {
        switch (channel_count) {
            case 1: return ColorSpace::eGray;
            case 2: return ColorSpace::eGrayAlpha;
            case 3: return ColorSpace::eRGB;
            case 4: return ColorSpace::eRGBA;
            default: return ColorSpace::eInvalid;
        }
    }

    static constexpr bool is_native(ColorSpace color_space) noexcept
    {
        return color_space != ColorSpace::eInvalid and decode(get_channel_count(color_space)) == color_space;
    }
};

template <>
struct enum_traits<ImageFormat>
{
    static constexpr ColorSpace get_color_space(ImageFormat format) noexcept
    {
        return static_cast<ColorSpace>(std::to_underlying(format) >> 8);
    }
    static constexpr PixelDataType get_pixel_data_type(ImageFormat format) noexcept
    {
        return static_cast<PixelDataType>(std::to_underlying(format) & 0xff);
    }
    static constexpr uint8_t get_channel_count(ImageFormat format) noexcept
    {
        return enum_traits<ColorSpace>::get_channel_count(get_color_space(format));
    }
    static constexpr uint8_t get_bytes_per_channel(ImageFormat format) noexcept
    {
        return enum_traits<PixelDataType>::get_bytes_per_channel(get_pixel_data_type(format));
    }
    static constexpr size_t pixel_size(ImageFormat format) noexcept
    {
        return static_cast<size_t>(get_channel_count(format)) * get_bytes_per_channel(format);
    }
    static constexpr ImageFormat get_image_format(ColorSpace color_space, PixelDataType data_type) noexcept
    {
        return static_cast<ImageFormat>(internal::encode(color_space, data_type));
    }
    static constexpr ImageFormat decode(ImageFormat format) noexcept
    {
        const auto color_space = enum_traits<ColorSpace>::decode(get_channel_count(format));
        return color_space == ColorSpace::eInvalid ? ImageFormat::eInvalid : get_image_format(color_space, get_pixel_data_type(format));
    }
    static constexpr ImageFormat decode_gpu_friendly(ImageFormat format) noexcept
    {
        const auto channel_count = get_channel_count(format);
        if (channel_count == 3) { return get_image_format(ColorSpace::eRGBA, get_pixel_data_type(format)); }
        return decode(format);
    }
    static constexpr ImageFormat deduce_format(PixelDataType data_type, int channel_count) noexcept
    {
        if (channel_count < 1 or channel_count > 4) { return ImageFormat::eInvalid; }
        const auto color_space = enum_traits<ColorSpace>::decode(channel_count);
        return color_space == ColorSpace::eInvalid ? ImageFormat::eInvalid : get_image_format(color_space, data_type);
    }
    static constexpr bool is_native(ImageFormat format) noexcept
    {
        return enum_traits<ColorSpace>::is_native(get_color_space(format));
    }
    static constexpr bool is_valid(ImageFormat format) noexcept
    {
        return format != ImageFormat::eInvalid and
            get_channel_count(format) >= 1 and
            get_channel_count(format) <= 4 and
            enum_traits<PixelDataType>::is_supported(get_pixel_data_type(format));
    }
};

} // namespace lcf::img
