#pragma once

#include "basic_data_enums.h"
#include <cstdint>
#include <utility>

namespace lcf::img {

enum class PixelDataType : uint8_t
{
    eUint8 = std::to_underlying(BasicDataType::eUint8),
    eUint16 = std::to_underlying(BasicDataType::eUint16),
    eUint32 = std::to_underlying(BasicDataType::eUint32),
    eFloat16 = std::to_underlying(BasicDataType::eFloat16),
    eFloat32 = std::to_underlying(BasicDataType::eFloat32),
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

enum class ImageFlags : uint8_t
{
    eNone = 0,
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

inline constexpr uint8_t encode(ColorSpace color_space, bool is_color_format, uint8_t channel_count) noexcept
{
    return (std::to_underlying(color_space) << 3) |
        (static_cast<uint8_t>(is_color_format) << 2) |
        (channel_count - 1);
}

} // namespace internal

enum class ColorSpace : uint8_t
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

inline constexpr uint16_t encode(lcf::img::ColorSpace color_space, PixelDataType data_type) noexcept
{
    return (static_cast<uint16_t>(color_space) << 8) | std::to_underlying(data_type);
}

} // namespace internal

enum class ImageFormat : uint16_t
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

} // namespace lcf::img
