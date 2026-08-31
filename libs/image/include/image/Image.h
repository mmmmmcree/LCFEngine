#pragma once

#include <image/enum/image_enums.h>
#include <image/enum/enum_traits.h>
#include <image/error.h>
#include <float16.h>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace lcf::img {

template <typename T>
concept image_channel_c = not std::is_reference_v<T> and not std::is_volatile_v<T> and (
    std::same_as<std::remove_const_t<T>, uint8_t> or
    std::same_as<std::remove_const_t<T>, uint16_t> or
    std::same_as<std::remove_const_t<T>, float16_t> or
    std::same_as<std::remove_const_t<T>, float>
);

class ImageInfo
{
    using Self = ImageInfo;
public:
    ~ImageInfo() noexcept = default;
    ImageInfo(const Self &) = default;
    Self & operator=(const Self &) = default;
    ImageInfo(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }
    ImageFileType getFileType() const noexcept { return m_file_type; }
    ImageFormat getEncodeFormat() const noexcept { return m_format; }
    uint8_t getChannelCount() const noexcept { return enum_traits<ImageFormat>::get_channel_count(m_format); }
    uint8_t getBytesPerChannel() const noexcept { return enum_traits<ImageFormat>::get_bytes_per_channel(m_format); }
    const std::filesystem::path & getPath() const noexcept { return m_path; }
private:
    ImageInfo(
        std::filesystem::path path,
        uint32_t width,
        uint32_t height,
        ImageFileType file_type,
        ImageFormat format) noexcept :
        m_path(std::move(path)),
        m_width(width),
        m_height(height),
        m_file_type(file_type),
        m_format(format) {}

    std::filesystem::path m_path;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ImageFileType m_file_type = ImageFileType::eInvalid;
    ImageFormat m_format = ImageFormat::eInvalid;
    friend std::expected<ImageInfo, Error> read_image_info(const std::filesystem::path & path) noexcept;
};

std::expected<ImageInfo, Error> read_image_info(const std::filesystem::path & path) noexcept;

class Image
{
    using Self = Image;
public:
    ~Image() noexcept = default;
    Image() noexcept = default;
    Image(uint32_t width, uint32_t height, ImageFormat format);
    Image(const Self &) = default;
    Self & operator=(const Self &) = default;
    Image(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code loadFromFile(const ImageInfo & info) noexcept;
    std::error_code loadFromFile(const ImageInfo & info, ImageFormat specific_format) noexcept;
    std::error_code loadFromFileGpuFriendly(const ImageInfo & info) noexcept;
    std::error_code loadFromFile(const std::filesystem::path & path) noexcept;
    std::error_code loadFromFileGpuFriendly(const std::filesystem::path & path) noexcept;
    std::error_code loadFromMemoryEncoded(std::span<const std::byte> data) noexcept;
    std::error_code loadFromMemoryPixels(std::span<const std::byte> data, uint32_t width, ImageFormat format) noexcept;
    std::error_code saveToFile(const std::filesystem::path & path) const noexcept;
    std::span<std::byte> getDataSpan() noexcept { return m_data; }
    std::span<const std::byte> getDataSpan() const noexcept { return m_data; }
    ImageFormat getDecodeFormat() const noexcept { return m_format; }
    ColorSpace getColorSpace() const noexcept { return enum_traits<ImageFormat>::get_color_space(m_format); }
    PixelDataType getDataType() const noexcept { return enum_traits<ImageFormat>::get_pixel_data_type(m_format); }
    uint32_t getChannelCount() const noexcept { return enum_traits<ImageFormat>::get_channel_count(m_format); }
    uint32_t getBytesPerChannel() const noexcept { return enum_traits<ImageFormat>::get_bytes_per_channel(m_format); }
    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }
    std::pair<uint32_t, uint32_t> getDimensions() const noexcept { return {m_width, m_height}; }
private:
    std::vector<std::byte> m_data;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ImageFormat m_format = ImageFormat::eInvalid;
};

} // namespace lcf::img
