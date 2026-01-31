#pragma once

#include <utility>

namespace lcf {
    enum PixelDataType : uint8_t // 5 bits for type size, 3 bits for enum index
    {
        eUint8   = 0 << 5 | 1,
        eUint16  = 1 << 5 | 2,
        eFloat16 = 2 << 5 | 2,
        eFloat32 = 3 << 5 | 4,
    };

    enum class ImageFileType : uint8_t
    {
        eInvalid,
        ePNG,
        eJPG,
        eHDR,
        eEXR,
        eTGA,
        eBMP,
    };

    enum class ImageFlags : uint8_t
    {
        eNone= 0,
        eSRGB = 1 << 0, 
        eLinear = 1 << 1,
        ePremult = 1 << 2,
    };

    enum class ColorSpace : uint16_t // 8 bits for enum index, 8 bits for channel count
    {
        eGray      = 1 << 8 | 1,  
        eRGB       = 3 << 8 | 3,  
        eBGR       = 4 << 8 | 3,  
        eRGBA      = 5 << 8 | 4,  
        eBGRA      = 6 << 8 | 4,  
        eARGB      = 7 << 8 | 4,  
        eCMYK      = 8 << 8 | 4,  
    };

namespace internal {
    constexpr uint32_t encode(PixelDataType data_type, ColorSpace color_space) noexcept
    {
        return (static_cast<uint32_t>(data_type) << 16) | std::to_underlying(color_space);
    }
}

    enum class ImageFormat : uint32_t // 8 bits for data type, 16 bits for color space, remains 8 bits for flags
    {
        eInvalid = 0,
        eGray8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eGray),
        eGray16Uint = internal::encode(PixelDataType::eUint16, ColorSpace::eGray),
        eGray32Float = internal::encode(PixelDataType::eFloat32, ColorSpace::eGray),
        eRGB8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eRGB),
        eRGB16Uint = internal::encode(PixelDataType::eUint16, ColorSpace::eRGB),
        eRGB32Float = internal::encode(PixelDataType::eFloat32, ColorSpace::eRGB),
        eBGR8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eBGR),
        eRGBA8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eRGBA),
        eRGBA16Uint = internal::encode(PixelDataType::eUint16, ColorSpace::eRGBA),
        eRGBA32Float = internal::encode(PixelDataType::eFloat32, ColorSpace::eRGBA),
        eBGRA8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eBGRA),
        eARGB8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eARGB),
        eCMYK8Uint = internal::encode(PixelDataType::eUint8, ColorSpace::eCMYK),
        eCMYK32Float = internal::encode(PixelDataType::eFloat32, ColorSpace::eCMYK),
    };
}

namespace lcf::enum_decode {
    constexpr ImageFormat get_image_format(ColorSpace color_space, PixelDataType data_type) noexcept
    {
        return static_cast<ImageFormat>((std::to_underlying(data_type) << 16) | std::to_underlying(color_space));
    }

    constexpr ColorSpace get_color_space(ImageFormat format) noexcept
    {
        return static_cast<ColorSpace>(std::to_underlying(format) & 0xFFFF);
    }

    constexpr PixelDataType get_pixel_data_type(ImageFormat format) noexcept
    {
        return static_cast<PixelDataType>((std::to_underlying(format) >> 16) & 0xFF);
    }

    constexpr uint8_t get_channel_count(ImageFormat format) noexcept
    {
        return std::to_underlying(get_color_space(format)) & 0xFF; 
    }

    constexpr uint8_t get_bytes_per_channel(ImageFormat format) noexcept
    {
        return get_pixel_data_type(format) & 0x1F; 
    }
}