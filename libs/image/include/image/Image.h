#pragma once

#include "image_fwd_decls.h"
#include "image_enums.h"
#include <filesystem>
#include "details/ImageVariant.h"
#include <span>

namespace lcf {
    class ImageInfo
    {
    public:
        ImageInfo(const std::filesystem::path & path);
        ~ImageInfo() noexcept = default;
        ImageInfo(ImageInfo &) = default;
        ImageInfo & operator=(const ImageInfo &) = default;
        ImageInfo(ImageInfo &&) noexcept = default;
        ImageInfo & operator=(ImageInfo &&) noexcept = default;
    public:
        uint32_t getWidth() const noexcept { return m_width; }
        uint32_t getHeight() const noexcept { return m_height; }
        ImageFileType getFileType() const noexcept { return m_file_type; }
        ImageFormat getEncodeFormat() const noexcept { return m_format; }
        uint8_t getChannelCount() const noexcept { return enum_decode::get_channel_count(m_format); }
        uint8_t getBytesPerChannel() const noexcept { return enum_decode::get_bytes_per_channel(m_format); }
        const std::filesystem::path & getPath() const noexcept { return m_path; }
    private:    
        std::filesystem::path m_path;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        ImageFileType m_file_type = ImageFileType::eInvalid;
        ImageFormat m_format = ImageFormat::eInvalid;
    };

    class Image : public ImagePointerDefs
    {
        using Self = Image;
        using ImageVariant = details::ImageVariant;
    public:
        Image() = default;
        Image(uint32_t width, uint32_t height, ImageFormat format);
        Image(const Image &) = default;
        Image(Image &&) noexcept = default;
        Image & operator=(const Image & other) = default;
        Image & operator=(Image && other) noexcept = default;
    private:
        Image(ImageVariant && image) noexcept;
        Image & operator=(ImageVariant && image);
    public:
        std::error_code convertTo(ImageFormat format) noexcept;
        Self & resize(uint32_t width, uint32_t height, ImageSampler sampler) noexcept;
        std::error_code loadFromFile(const ImageInfo & info) noexcept;
        std::error_code loadFromFile(const ImageInfo & info, ImageFormat specific_format) noexcept;
        std::error_code loadFromFileGpuFriendly(const ImageInfo & info) noexcept; //- RGB -> RGBA
        std::error_code loadFromMemory(std::span<const std::byte> data, ImageFormat format, uint32_t width) noexcept;
        std::error_code saveToFile(const std::filesystem::path & path) const noexcept;
        std::span<std::byte> getDataSpan() noexcept;
        std::span<const std::byte> getDataSpan() const noexcept;
        ImageFormat getDecodeFormat() const noexcept { return m_format; }
        ColorSpace getColorSpace() const noexcept { return enum_decode::get_color_space(m_format); }
        PixelDataType getDataType() const noexcept { return enum_decode::get_pixel_data_type(m_format); }
        uint32_t getChannelCount() const noexcept { return enum_decode::get_channel_count(m_format); }
        uint32_t getBytesPerChannel() const noexcept { return enum_decode::get_bytes_per_channel(m_format); }
        uint32_t getWidth() const noexcept { return static_cast<uint32_t>(m_image.width()); }
        uint32_t getHeight() const noexcept { return static_cast<uint32_t>(m_image.height()); }
        std::pair<uint32_t, uint32_t> getDimensions() const noexcept { return { this->getWidth(), this->getHeight() }; }
    private:
        ImageVariant m_image;
        ImageFormat m_format;
    };
}