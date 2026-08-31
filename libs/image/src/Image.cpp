#include "image/Image.h"

#include <cctype>
#include <limits>
#include <memory>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

using namespace lcf::img;

namespace {

struct DecodedResult
{
    uint32_t width;
    uint32_t height;
    std::vector<std::byte> data;
};

ImageFileType get_file_type(const std::filesystem::path & path) noexcept;

std::expected<DecodedResult, std::error_code> decode_file(const std::filesystem::path & path, ImageFormat format) noexcept;

std::expected<DecodedResult, std::error_code> decode_memory(std::span<const std::byte> encoded, ImageFormat format) noexcept;

std::expected<DecodedResult, std::error_code> copy_decoded(void * pixels, int width, int height, ImageFormat format) noexcept;

} // namespace

namespace lcf::img {

std::expected<ImageInfo, Error> read_image_info(const std::filesystem::path & path) noexcept
{
    const auto file_type = get_file_type(path);
    if (file_type == ImageFileType::eInvalid) {
        return std::unexpected(Error {make_error_code(errc::invalid_file_type), path.string()});
    }
    std::error_code file_error;
    if (not std::filesystem::exists(path, file_error) or file_error) {
        return std::unexpected(Error {make_error_code(errc::file_not_found), path.string()});
    }
    const auto path_string = path.string();
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info(path_string.c_str(), &width, &height, &channels) == 0 or width <= 0 or height <= 0) {
        return std::unexpected(Error {make_error_code(errc::invalid_image), path_string});
    }
    const auto data_type = stbi_is_hdr(path_string.c_str()) ? PixelDataType::eFloat32 :
        stbi_is_16_bit(path_string.c_str()) ? PixelDataType::eUint16 : PixelDataType::eUint8;
    const auto format = enum_traits<ImageFormat>::deduce_format(data_type, channels);
    if (format == ImageFormat::eInvalid) {
        return std::unexpected(Error {make_error_code(errc::unsupported_format), path_string});
    }
    return ImageInfo {path, static_cast<uint32_t>(width), static_cast<uint32_t>(height), file_type, format};
}

Image::Image(uint32_t width, uint32_t height, ImageFormat format) :
    m_width(width),
    m_height(height),
    m_format(enum_traits<ImageFormat>::decode(format))
{
    if (not enum_traits<ImageFormat>::is_valid(m_format) or width == 0 or height == 0) {
        m_width = m_height = 0;
        m_format = ImageFormat::eInvalid;
        return;
    }
    m_data.resize(static_cast<size_t>(width) * height * enum_traits<ImageFormat>::pixel_size(m_format));
}

std::error_code Image::loadFromFile(const ImageInfo & info) noexcept
{
    return this->loadFromFile(info, info.getEncodeFormat());
}

std::error_code Image::loadFromFile(const ImageInfo & info, ImageFormat specific_format) noexcept
{
    const auto format = enum_traits<ImageFormat>::decode(specific_format);
    if (info.getFileType() == ImageFileType::eInvalid or not enum_traits<ImageFormat>::is_valid(format)) {
        return make_error_code(errc::unsupported_format);
    }
    auto expected_decoded = decode_file(info.getPath(), format);
    if (not expected_decoded) { return expected_decoded.error(); }
    const auto & decoded = expected_decoded.value();
    m_data = std::move(decoded.data);
    m_width = decoded.width;
    m_height = decoded.height;
    m_format = format;
    return {};
}

std::error_code Image::loadFromFileGpuFriendly(const ImageInfo & info) noexcept
{
    return this->loadFromFile(info, enum_traits<ImageFormat>::decode_gpu_friendly(info.getEncodeFormat()));
}

std::error_code Image::loadFromFile(const std::filesystem::path & path) noexcept
{
    const auto expected_info = read_image_info(path);
    if (not expected_info) { return expected_info.error().code(); }
    return this->loadFromFile(*expected_info);
}

std::error_code Image::loadFromFileGpuFriendly(const std::filesystem::path & path) noexcept
{
    const auto expected_info = read_image_info(path);
    if (not expected_info) { return expected_info.error().code(); }
    return this->loadFromFileGpuFriendly(*expected_info);
}

std::error_code Image::loadFromMemoryEncoded(std::span<const std::byte> data) noexcept
{
    if (data.empty() or data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return std::make_error_code(std::errc::invalid_argument);
    }
    const auto * bytes = reinterpret_cast<const stbi_uc *>(data.data());
    const auto size = static_cast<int>(data.size());
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(bytes, size, &width, &height, &channels) == 0) {
        return std::make_error_code(std::errc::invalid_argument);
    }
    const auto data_type = stbi_is_hdr_from_memory(bytes, size) ? PixelDataType::eFloat32 :
        stbi_is_16_bit_from_memory(bytes, size) ?  PixelDataType::eUint16 : PixelDataType::eUint8;
    const auto format = enum_traits<ImageFormat>::deduce_format(data_type, channels);
    if (format == ImageFormat::eInvalid) { return make_error_code(errc::unsupported_format); }
    auto expected_decoded = decode_memory(data, format);
    if (not expected_decoded) { return expected_decoded.error(); }
    m_data = std::move(expected_decoded->data);
    m_width = expected_decoded->width;
    m_height = expected_decoded->height;
    m_format = format;
    return {};
}

std::error_code Image::loadFromMemoryPixels(std::span<const std::byte> data, uint32_t width, ImageFormat format) noexcept
{
    format = enum_traits<ImageFormat>::decode(format);
    const auto row_size = static_cast<size_t>(width) * enum_traits<ImageFormat>::pixel_size(format);
    if (row_size == 0 or data.size() % row_size != 0) { return std::make_error_code(std::errc::invalid_argument); }
    m_data.assign_range(data);
    m_width = width;
    m_height = static_cast<uint32_t>(data.size() / row_size);
    m_format = format;
    return {};
}

std::error_code Image::saveToFile(const std::filesystem::path & path) const noexcept
{
    const auto file_type = get_file_type(path);
    const auto path_string = path.string();
    const auto channels = enum_traits<ImageFormat>::get_channel_count(m_format);
    bool success = false;
    int ec = 1;
    switch (file_type) {
        case ImageFileType::ePNG: { ec = stbi_write_png(path_string.c_str(), m_width, m_height, channels, m_data.data(), 0);  } break;
        case ImageFileType::eJPEG: { ec = stbi_write_jpg(path_string.c_str(), m_width, m_height, channels, m_data.data(), 100); } break;
        case ImageFileType::eBMP: { ec = stbi_write_bmp(path_string.c_str(), m_width, m_height, channels, m_data.data()); } break;
        case ImageFileType::eTGA: { ec = stbi_write_tga(path_string.c_str(), m_width, m_height, channels, m_data.data()); } break;
        case ImageFileType::eHDR: { ec = stbi_write_hdr(path_string.c_str(), m_width, m_height, channels, reinterpret_cast<const float *>(m_data.data())); } break;
        default: return std::make_error_code(std::errc::invalid_argument);
    }
    return ec ? std::make_error_code(std::errc::io_error) : std::error_code {};
}

} // namespace lcf::img

namespace {

ImageFileType get_file_type(const std::filesystem::path & path) noexcept
{
    auto extension = path.extension().string();
    for (char & character : extension) {
        const auto value = static_cast<unsigned char>(character);
        if (std::isupper(value)) { character = static_cast<char>(std::tolower(value)); }
    }
    if (extension == ".png") { return ImageFileType::ePNG; }
    if (extension == ".jpg" or extension == ".jpeg") { return ImageFileType::eJPEG; }
    if (extension == ".hdr") { return ImageFileType::eHDR; }
    if (extension == ".exr") { return ImageFileType::eEXR; }
    if (extension == ".tga") { return ImageFileType::eTGA; }
    if (extension == ".bmp") { return ImageFileType::eBMP; }
    return ImageFileType::eInvalid;
}

std::expected<DecodedResult, std::error_code> decode_file(const std::filesystem::path & path, ImageFormat format) noexcept
{
    const auto path_string = path.string();
    int width = 0;
    int height = 0;
    int source_channels = 0;
    const auto channels = enum_traits<ImageFormat>::get_channel_count(format);
    const auto data_type = enum_traits<ImageFormat>::get_pixel_data_type(format);
    void * pixels = nullptr;
    switch (data_type) {
        case PixelDataType::eUint8: { pixels = stbi_load(path_string.c_str(), &width, &height, &source_channels, channels); } break;
        case PixelDataType::eUint16: { pixels = stbi_load_16(path_string.c_str(), &width, &height, &source_channels, channels); } break;
        case PixelDataType::eFloat32: { pixels = stbi_loadf(path_string.c_str(), &width, &height, &source_channels, channels); } break;
        default: { return std::unexpected(make_error_code(errc::unsupported_format)); }
    }
    if (not pixels or width <= 0 or height <= 0) { return std::unexpected(std::make_error_code(std::errc::io_error)); }
    return copy_decoded(pixels, width, height, format);
}

std::expected<DecodedResult, std::error_code> decode_memory(std::span<const std::byte> encoded, ImageFormat format) noexcept
{
    const auto * bytes = reinterpret_cast<const stbi_uc *>(encoded.data());
    const auto size = static_cast<int>(encoded.size());
    int width = 0;
    int height = 0;
    int source_channels = 0;
    const auto channels = enum_traits<ImageFormat>::get_channel_count(format);
    const auto data_type = enum_traits<ImageFormat>::get_pixel_data_type(format);
    void * pixels = nullptr;
    switch (data_type) {
        case PixelDataType::eUint8: { pixels = stbi_load_from_memory(bytes, size, &width, &height, &source_channels, channels); } break;
        case PixelDataType::eUint16: { pixels = stbi_load_16_from_memory(bytes, size, &width, &height, &source_channels, channels); } break;
        case PixelDataType::eFloat32: { pixels = stbi_loadf_from_memory(bytes, size, &width, &height, &source_channels, channels); } break;
        default: { return std::unexpected(make_error_code(errc::unsupported_format)); }
    }
    if (not pixels or width <= 0 or height <= 0) { return std::unexpected(std::make_error_code(std::errc::io_error)); }
    return copy_decoded(pixels, width, height, format);
}

std::expected<DecodedResult, std::error_code> copy_decoded(void * pixels, int width, int height, ImageFormat format) noexcept
{
    std::unique_ptr<void, decltype(&stbi_image_free)> owned_pixels {pixels, stbi_image_free};
    std::span<const std::byte> data {static_cast<const std::byte *>(pixels), width * height * enum_traits<ImageFormat>::pixel_size(format)};
    return DecodedResult {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        {data.begin(), data.end()}
    };
}

} // namespace
