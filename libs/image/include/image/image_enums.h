#pragma once

#include <utility>
#include "basic_data_enums.h"

namespace lcf {
    enum class PixelDataType : uint8_t
    {
        eUint8 = BasicDataType::eUint8,
        eUint16 = BasicDataType::eUint16,
        eUint32 = BasicDataType::eUint32,
        eFloat16 = BasicDataType::eFloat16,
        eFloat32 = BasicDataType::eFloat32,
    };

    enum class ImageFileType : uint8_t
    {
        eInvalid,
        ePNG,
        eJPEG,
        eHDR,
        eEXR,
        eTGA,
        eBMP,
    };

    enum class ImageSampler : uint8_t
    {
        eNearest,
        eLinear,
    };

    enum class ImageFlags : uint8_t
    {
        eNone= 0,
        eSRGB = 1 << 0, 
        eLinear = 1 << 1,
        ePremult = 1 << 2,
    };

namespace internal {
    enum class ColorSpace : uint8_t
    {
        eInvalid = 0,
        eGray,  
        eGrayAlpha,  
        eRGB,  
        eBGR,  
        eRGBA,  
        eBGRA,  
        eARGB,  
        eABGR,
        eCMYK,
        eYCbCr,
        eYCCK,
    };

    inline constexpr uint8_t encode(ColorSpace color_space, bool is_color_format, uint8_t channel_count)
    {
        return (std::to_underlying(color_space) << 3) | (static_cast<uint8_t>(is_color_format) << 2) | (channel_count - 1);
    }
}

    enum class ColorSpace : uint8_t // 6 bits for enum index, 2 bits for channel count
    {
        eInvalid = 0,
        eGray = internal::encode(internal::ColorSpace::eGray, false, 1),   
        eGrayAlpha = internal::encode(internal::ColorSpace::eGrayAlpha, false, 2),   
        eRGB = internal::encode(internal::ColorSpace::eRGB, true, 3),   
        eBGR = internal::encode(internal::ColorSpace::eBGR, true, 3),   
        eRGBA = internal::encode(internal::ColorSpace::eRGBA, true, 4),   
        eBGRA = internal::encode(internal::ColorSpace::eBGRA, true, 4),  
        eARGB = internal::encode(internal::ColorSpace::eARGB, true, 4),  
        eABGR = internal::encode(internal::ColorSpace::eABGR, true, 4),  
        eCMYK = internal::encode(internal::ColorSpace::eCMYK, true, 4),  
        eYCbCr = internal::encode(internal::ColorSpace::eYCbCr, true, 3),
        eYCCK = internal::encode(internal::ColorSpace::eYCCK, true, 4),
    };

namespace internal {
    inline constexpr uint16_t encode(lcf::ColorSpace color_space, PixelDataType data_type) noexcept
    {
        return (static_cast<uint16_t>(color_space) << 8) | std::to_underlying(data_type);
    }
}

    enum class ImageFormat : uint16_t // 8 bits for data type, 16 bits for color space, remains 8 bits for flags
    {
        eInvalid = 0,

        eGray8Uint = internal::encode(ColorSpace::eGray, PixelDataType::eUint8),
        eGray16Uint = internal::encode(ColorSpace::eGray, PixelDataType::eUint16),
        eGray16Float = internal::encode(ColorSpace::eGray, PixelDataType::eFloat16),
        eGray32Float = internal::encode(ColorSpace::eGray, PixelDataType::eFloat32),

        eGrayAlpha8Uint = internal::encode(ColorSpace::eGrayAlpha, PixelDataType::eUint8),
        eGrayAlpha16Uint = internal::encode(ColorSpace::eGrayAlpha, PixelDataType::eUint16),
        eGrayAlpha16Float = internal::encode(ColorSpace::eGrayAlpha, PixelDataType::eFloat16),
        eGrayAlpha32Float = internal::encode(ColorSpace::eGrayAlpha, PixelDataType::eFloat32),

        eRGB8Uint = internal::encode(ColorSpace::eRGB, PixelDataType::eUint8),
        eRGB16Uint = internal::encode(ColorSpace::eRGB, PixelDataType::eUint16),
        eRGB16Float = internal::encode(ColorSpace::eRGB, PixelDataType::eFloat16),
        eRGB32Float = internal::encode(ColorSpace::eRGB, PixelDataType::eFloat32),

        eRGBA8Uint = internal::encode(ColorSpace::eRGBA, PixelDataType::eUint8),
        eRGBA16Uint = internal::encode(ColorSpace::eRGBA, PixelDataType::eUint16),
        eRGBA16Float = internal::encode(ColorSpace::eRGBA, PixelDataType::eFloat16),
        eRGBA32Float = internal::encode(ColorSpace::eRGBA, PixelDataType::eFloat32),

        eBGR8Uint = internal::encode(ColorSpace::eBGR, PixelDataType::eUint8),
        eBGRA8Uint = internal::encode(ColorSpace::eBGRA, PixelDataType::eUint8),
        eARGB8Uint = internal::encode(ColorSpace::eARGB, PixelDataType::eUint8),
        eCMYK8Uint = internal::encode(ColorSpace::eCMYK, PixelDataType::eUint8),
        eYCbCr8Uint = internal::encode(ColorSpace::eYCbCr, PixelDataType::eUint8),
        eYCCK8Uint = internal::encode(ColorSpace::eYCCK, PixelDataType::eUint8),
    };
}

namespace lcf::enum_decode {
    inline constexpr ColorSpace get_color_space(ImageFormat format) noexcept
    {
        return static_cast<ColorSpace>(std::to_underlying(format) >> 8);
    }

    inline constexpr bool is_color_format(ImageFormat format) noexcept
    {
        return (std::to_underlying(get_color_space(format)) >> 2) & 1;
    }

    inline constexpr ImageFormat get_image_format(ColorSpace color_space, PixelDataType data_type) noexcept
    {
        return static_cast<ImageFormat>(internal::encode(color_space, data_type));
    }

    inline constexpr PixelDataType get_pixel_data_type(ImageFormat format) noexcept
    {
        return static_cast<PixelDataType>(std::to_underlying(format)& 0xFF);
    }

    inline constexpr uint8_t get_channel_count(ColorSpace color_space) noexcept
    {
        return (std::to_underlying(color_space) & 0x3) + 1; 
    }

    inline constexpr uint8_t get_channel_count(ImageFormat format) noexcept
    {
        return get_channel_count(get_color_space(format)); 
    }

    inline constexpr uint8_t get_bytes_per_channel(ImageFormat format) noexcept
    {
        return get_size_in_bytes(static_cast<BasicDataType>(get_pixel_data_type(format)));
    }

    inline constexpr ColorSpace decode_color_space(uint8_t channel_count) noexcept
    {
        ColorSpace color_space = ColorSpace::eInvalid;
        switch (channel_count) {
            case 1: { color_space = ColorSpace::eGray; } break;
            case 2: { color_space = ColorSpace::eGrayAlpha; } break;
            case 3: { color_space = ColorSpace::eRGB; } break;
            case 4: { color_space = ColorSpace::eRGBA; } break;
            default: break;
        }
        return color_space;
    }

    inline constexpr ImageFormat decode(ImageFormat encode) noexcept
    {
        ImageFormat decoded = encode;
        switch (encode) {
            case ImageFormat::eBGR8Uint:
            case ImageFormat::eCMYK8Uint:
            case ImageFormat::eYCbCr8Uint:
            case ImageFormat::eYCCK8Uint: { decoded = ImageFormat::eRGB8Uint; } break;
            case ImageFormat::eBGRA8Uint: 
            case ImageFormat::eARGB8Uint: { decoded = ImageFormat::eRGBA8Uint; } break;
            default: break;
        }
        return decoded;
    }

    inline constexpr ImageFormat decode_gpu_friendly(ImageFormat encode) noexcept
    {
        ImageFormat decoded = decode(encode);
        if (decode_color_space(get_channel_count(decoded)) == ColorSpace::eRGB) {
            return static_cast<ImageFormat>(internal::encode(ColorSpace::eRGBA, get_pixel_data_type(decoded)));
        }
        return decoded;
    }

    inline constexpr ImageFormat decode_image_format(PixelDataType pixel_data_type, uint8_t channel_count) noexcept
    {
        return get_image_format(decode_color_space(channel_count), pixel_data_type);
    }
}