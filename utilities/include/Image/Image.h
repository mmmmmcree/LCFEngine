#pragma once

#include "PointerDefs.h"
#include <filesystem>
#include <span>
#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/dynamic_image_all.hpp>


namespace lcf {
    class ImageView;
    class ConstImageView;

    class Image : public STDPointerDefs<Image>
    {
        friend class ImageView;
        friend class ConstImageView;
        using Self = Image;
    public:
        using NullImage = boost::gil::gray8s_image_t;
        using ImageVariant = boost::gil::any_image<
            NullImage,

            boost::gil::gray8_image_t,
            boost::gil::gray16_image_t,
            boost::gil::gray32f_image_t,
            
            boost::gil::rgb8_image_t,
            boost::gil::rgb16_image_t,
            boost::gil::rgb32f_image_t,
            
            boost::gil::rgba8_image_t,
            boost::gil::rgba16_image_t,
            boost::gil::rgba32f_image_t,
            
            boost::gil::bgr8_image_t,
            boost::gil::bgra8_image_t,
            
            boost::gil::argb8_image_t,
            
            boost::gil::cmyk8_image_t,
            boost::gil::cmyk32f_image_t>;
        using GrayImageVariant = boost::gil::any_image<
            boost::gil::gray8_image_t,
            boost::gil::gray16_image_t,
            boost::gil::gray32f_image_t>;
        using ColorImageVariant = boost::gil::any_image<
            boost::gil::rgb8_image_t,
            boost::gil::rgb16_image_t,
            boost::gil::rgb32f_image_t,
            boost::gil::rgba8_image_t,
            boost::gil::rgba16_image_t,
            boost::gil::rgba32f_image_t,
            boost::gil::bgr8_image_t,
            boost::gil::bgra8_image_t,
            boost::gil::argb8_image_t,
            boost::gil::cmyk8_image_t,
            boost::gil::cmyk32f_image_t>;
        enum DataType : uint8_t // 5 bits for type size, 3 bits for enum index
        {
            eUint8   = 0 << 5 | 1,
            eUint16  = 1 << 5 | 2,
            eFloat16 = 2 << 5 | 2,
            eFloat32 = 3 << 5 | 4,
        };
        enum class FileType : uint8_t
        {
            eInvalid,
            ePNG,
            eJPG,
            eHDR,
            eEXR,
            eTGA,
            eBMP,
        };
        enum class Flags : uint8_t
        {
            eNone= 0,
            eSRGB = 1 << 0, 
            eLinear = 1 << 1,
            ePremult = 1 << 2,
        };
        enum class ColorSpace : uint16_t // 8 bits for enum index, 8 bits for channel count
        {
            eGray      = 1 << 8 | 1,  // 0x0101 - 灰度
            eRGB       = 3 << 8 | 3,  // 0x0303 - RGB
            eBGR       = 4 << 8 | 3,  // 0x0403 - BGR
            eRGBA      = 5 << 8 | 4,  // 0x0504 - RGBA
            eBGRA      = 6 << 8 | 4,  // 0x0604 - BGRA
            eARGB      = 7 << 8 | 4,  // 0x0704 - ARGB
            eCMYK      = 8 << 8 | 4,  // 0x0804 - CMYK
        };
        enum class Format : uint32_t // 8 bits for data type, 16 bits for color space, remains 8 bits for flags
        {
            eInvalid = 0,
            eGray8Uint   = (eUint8   << 16) | std::to_underlying(ColorSpace::eGray),      // 0x010101
            eGray16Uint  = (eUint16  << 16) | std::to_underlying(ColorSpace::eGray),      // 0x220101
            eGray32Float = (eFloat32 << 16) | std::to_underlying(ColorSpace::eGray),      // 0x680101
            eRGB8Uint    = (eUint8   << 16) | std::to_underlying(ColorSpace::eRGB),       // 0x010303
            eRGB16Uint   = (eUint16  << 16) | std::to_underlying(ColorSpace::eRGB),       // 0x220303
            eRGB32Float  = (eFloat32 << 16) | std::to_underlying(ColorSpace::eRGB),       // 0x680303
            eBGR8Uint    = (eUint8   << 16) | std::to_underlying(ColorSpace::eBGR),       // 0x010403
            eRGBA8Uint   = (eUint8   << 16) | std::to_underlying(ColorSpace::eRGBA),      // 0x010504
            eRGBA16Uint  = (eUint16  << 16) | std::to_underlying(ColorSpace::eRGBA),      // 0x220504
            eRGBA32Float = (eFloat32 << 16) | std::to_underlying(ColorSpace::eRGBA),      // 0x680504
            eBGRA8Uint   = (eUint8   << 16) | std::to_underlying(ColorSpace::eBGRA),      // 0x010604
            eARGB8Uint   = (eUint8   << 16) | std::to_underlying(ColorSpace::eARGB),      // 0x010704
            eCMYK8Uint   = (eUint8   << 16) | std::to_underlying(ColorSpace::eCMYK),      // 0x010804
            eCMYK32Float = (eFloat32 << 16) | std::to_underlying(ColorSpace::eCMYK),      // 0x680804
        };
    public:
        Image() = default;
        Image(const Image & other) = default;
        Image(Image && other) = default;
        Image & operator=(const Image & other) = default;
        Image & operator=(Image && other) = default;
        Image(ImageVariant && image) noexcept : m_image(std::move(image)) {}
        Image & operator=(ImageVariant && image) noexcept { m_image = std::move(image); return *this; }
        Format getFormat() const noexcept { return m_format; }
        ColorSpace getColorSpace() const noexcept { return get_color_space(m_format); }
        DataType getDataType() const noexcept { return get_data_type(m_format); }
        uint32_t getChannelCount() const noexcept { return get_channel_count(m_format); }
        uint32_t getBytesPerChannel() const noexcept { return get_bytes_per_channel(m_format); }
        uint32_t getWidth() const noexcept;
        uint32_t getHeight() const noexcept;
        std::pair<uint32_t, uint32_t> getDimensions() const noexcept { return { this->getWidth(), this->getHeight() }; }
        ImageView getImageView() noexcept;
        ConstImageView getImageView() const noexcept;
        std::span<std::byte> getInterleavedDataSpan() noexcept;
        std::span<const std::byte> getInterleavedDataSpan() const noexcept;
        bool loadFromFile(const std::filesystem::path & path) noexcept;
        bool loadFromFile(const std::filesystem::path & path, Format format) noexcept;
        bool loadFromMemory(std::span<const std::byte> data, Format format, size_t width) noexcept;
        bool saveToFile(const std::filesystem::path & path) const noexcept;
        Self & recreate(size_t width, size_t height, size_t alignment = 0);
        Self & convertTo(Format format);
        Self & flipUpDown();
        Self & flipLeftRight();
    private:
        static Format get_format(ColorSpace color_space, DataType data_type) { return static_cast<Format>((std::to_underlying(data_type) << 16) | std::to_underlying(color_space)); }
        static ColorSpace get_color_space(Format format) { return static_cast<ColorSpace>(std::to_underlying(format) & 0xFFFF); }
        static DataType get_data_type(Format format) { return static_cast<DataType>((std::to_underlying(format) >> 16) & 0xFF); }
        static size_t get_channel_count(Format format) { return std::to_underlying(get_color_space(format)) & 0xFF; }
        static size_t get_bytes_per_channel(Format format) { return get_data_type(format) & 0x1F; }
        static std::filesystem::path get_extension(FileType type);
        static FileType deduce_file_type(const std::filesystem::path & path);
    private:
        bool loadFromFileSTB(const std::filesystem::path &path) noexcept;
        bool loadFromFileSTB(const std::filesystem::path &path, Format format) noexcept;
        bool loadFromPNG(const std::filesystem::path & path) noexcept;
        bool loadFromPNG(const std::filesystem::path & path, Format format) noexcept;
        bool loadFromJPG(const std::filesystem::path & path) noexcept;
        bool loadFromJPG(const std::filesystem::path & path, Format format) noexcept;
        void updateFormat();
    private:
        Format m_format = Format::eInvalid;
        ImageVariant m_image = NullImage{};
    };
}