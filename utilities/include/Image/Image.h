#pragma once

#include "image_enums.h"
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
    public:
        Image() = default;
        Image(uint32_t width, uint32_t height, ImageFormat format);
        Image(const Image & other) = default;
        Image(Image && other) = default;
        Image & operator=(const Image & other) = default;
        Image & operator=(Image && other) = default;
        Image(ImageVariant && image) noexcept : m_image(std::move(image)) {}
        Image & operator=(ImageVariant && image) noexcept { m_image = std::move(image); return *this; }
        ImageFormat getFormat() const noexcept { return m_format; }
        ColorSpace getColorSpace() const noexcept { return enum_decode::get_color_space(m_format); }
        PixelDataType getDataType() const noexcept { return enum_decode::get_pixel_data_type(m_format); }
        uint32_t getChannelCount() const noexcept { return enum_decode::get_channel_count(m_format); }
        uint32_t getBytesPerChannel() const noexcept { return enum_decode::get_bytes_per_channel(m_format); }
        uint32_t getWidth() const noexcept;
        uint32_t getHeight() const noexcept;
        std::pair<uint32_t, uint32_t> getDimensions() const noexcept { return { this->getWidth(), this->getHeight() }; }
        ImageView getImageView() noexcept;
        ConstImageView getImageView() const noexcept;
        std::span<std::byte> getInterleavedDataSpan() noexcept;
        std::span<const std::byte> getInterleavedDataSpan() const noexcept;
        bool loadFromFile(const std::filesystem::path & path) noexcept;
        bool loadFromFile(const std::filesystem::path & path, ImageFormat format) noexcept;
        bool loadFromMemory(std::span<const std::byte> data, ImageFormat format, size_t width) noexcept;
        bool saveToFile(const std::filesystem::path & path) const noexcept;
        Self & recreate(size_t width, size_t height, size_t alignment = 0);
        Self & convertTo(ImageFormat format);
        Self & flipUpDown();
        Self & flipLeftRight();
    private:
        static std::filesystem::path get_extension(ImageFileType type);
        static ImageFileType deduce_file_type(const std::filesystem::path & path);
    private:
        bool loadFromFileSTB(const std::filesystem::path &path) noexcept;
        bool loadFromFileSTB(const std::filesystem::path &path, ImageFormat format) noexcept;
        bool loadFromPNG(const std::filesystem::path & path) noexcept;
        bool loadFromPNG(const std::filesystem::path & path, ImageFormat format) noexcept;
        bool loadFromJPG(const std::filesystem::path & path) noexcept;
        bool loadFromJPG(const std::filesystem::path & path, ImageFormat format) noexcept;
        void updateFormat();
    private:
        ImageFormat m_format = ImageFormat::eInvalid;
        ImageVariant m_image = NullImage{};
    };
}