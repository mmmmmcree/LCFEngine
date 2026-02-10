#include "image/Image.h"
#include "image/details/utils.h"
#include "image/details/convert.h"
#include "enums/enum_name.h"
#include "enums/enum_values.h"
#include "string_view_hash.h"
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/targa.hpp>
#include <boost/gil/extension/io/bmp.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/algorithm/string.hpp>
#include "log.h"
#include "span_cast.h"
#include <expected>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

using namespace lcf;
namespace gil = boost::gil;
namespace variant2 = boost::variant2;
using ImageVariant = details::ImageVariant<>;
using ConstImageViewVariant = details::ConstImageViewVariant;

//- auxiliary structs begin
struct ImageInfoData
{
    ImageInfoData() = default;
    ImageInfoData(uint32_t width, uint32_t height, ImageFormat format) :
        m_width(width),
        m_height(height),
        m_format(format)
    {}
    ~ImageInfoData() noexcept = default;
    ImageInfoData(const ImageInfoData &) = default;
    ImageInfoData(ImageInfoData &&) noexcept = default;
    ImageInfoData & operator=(const ImageInfoData &) = default;
    ImageInfoData & operator=(ImageInfoData &&) noexcept = default;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ImageFormat m_format = ImageFormat::eInvalid;
};
//- auxiliary structs end

//- auxiliary function forward declarations begin
std::filesystem::path get_extension(ImageFileType type) noexcept;

ImageFileType get_file_type(const std::filesystem::path & path) noexcept;

ImageInfoData read_info_from_png(const std::filesystem::path & path);

ImageInfoData read_info_from_jpeg(const std::filesystem::path & path);

bool is_stb_load_supported(const ImageInfo & info) noexcept;

std::expected<ImageVariant, std::error_code> load_from_file_gil(const ImageInfo & info, ImageFormat specific_format) noexcept;

std::expected<ImageVariant, std::error_code> load_from_file_stb(const ImageInfo & info, ImageFormat specific_format) noexcept;

std::expected<ImageVariant, std::error_code> load_from_memory_stb(std::span<const std::byte> data, ImageFormat & format) noexcept;

bool is_stb_save_supported(ImageFileType file_type) noexcept;

std::error_code save_to_file_stb(const ImageVariant & image, ImageFormat format, ImageFileType file_type, const std::filesystem::path & path) noexcept;

//- auxiliary function forward declarations end

ImageInfo::ImageInfo(const std::filesystem::path & path)
{
    if (not std::filesystem::exists(path)) {
        lcf_log_error("Image file not found: {}", path.string());
        return;
    }
    m_file_type = get_file_type(path);
    if (m_file_type == ImageFileType::eInvalid) {
        lcf_log_error("Invalid image file type: {}", path.string());
        return;
    }
    ImageInfoData data;
    switch (m_file_type) {
        case ImageFileType::ePNG: { data = read_info_from_png(path); } break;
        case ImageFileType::eJPEG: { data = read_info_from_jpeg(path); } break;
        default: break;
    }
    
    m_width = data.m_width;
    m_height = data.m_height;
    m_format = data.m_format;
    m_path = path;
}

Image::Image(uint32_t width, uint32_t height, ImageFormat format)
{
    m_format = enum_decode::decode(format);
    m_image = details::generate_image<>(width, height, m_format);
}

Image::Image(ImageVariant && image) noexcept :
    m_image(std::move(image))
{
}

Image & Image::operator=(ImageVariant && image)
{
    m_image = std::move(image);
    return *this;
}

std::error_code Image::convertTo(ImageFormat format) noexcept
{
    format = enum_decode::decode(format);
    if (m_format == format) { return {}; }
    auto new_image = details::generate_image<>(this->getWidth(), this->getHeight(), format);
    try {
        details::convert(details::view(m_image), details::view(new_image));
    } catch (const std::exception & e) {
        return std::make_error_code(std::errc::invalid_argument);
    }
    m_image = std::move(new_image);
    m_format = format;
    return {};
}

std::error_code lcf::Image::convertToGpuFriendly() noexcept
{
    return this->convertTo(enum_decode::decode_gpu_friendly(m_format));
}

Image & Image::resize(uint32_t width, uint32_t height, ImageSampler sampler) noexcept
{
    if (width == this->getWidth() and height == this->getHeight()) { return *this; }
    variant2::visit([width, height, sampler](auto && image) {
        using image_t = std::decay_t<decltype(image)>;
        image_t new_image {width, height};
        auto image_const_view = gil::const_view(image);
        auto new_image_view = gil::view(new_image);
        switch (sampler) {
            case ImageSampler::eNearest: { gil::resize_view(image_const_view, new_image_view, gil::nearest_neighbor_sampler{}); } break;
            case ImageSampler::eLinear: { gil::resize_view(image_const_view, new_image_view, gil::bilinear_sampler{}); } break;
            default: break;
        }
        image = std::move(new_image);
    }, m_image);
    return *this;
}

std::error_code Image::loadFromFile(const ImageInfo &info) noexcept
{
    return this->loadFromFile(info, info.getEncodeFormat());
}

std::error_code Image::loadFromFile(const ImageInfo &info, ImageFormat specific_format) noexcept
{
    ImageFormat format = enum_decode::decode(specific_format);
    if (info.getFileType() == ImageFileType::eInvalid) {
        return std::make_error_code(std::errc::invalid_argument);
    }
    std::expected<ImageVariant, std::error_code> expected;
    if (is_stb_load_supported(info)) {
        expected = load_from_file_stb(info, format);
    } else {
        expected = load_from_file_gil(info, format);
    }
    if (not expected) { return expected.error(); }
    else { m_image = std::move(*expected); }
    m_format = format;
    return {};
}

std::error_code Image::loadFromFileGpuFriendly(const ImageInfo &info) noexcept
{
    return this->loadFromFile(info, enum_decode::decode_gpu_friendly(info.getEncodeFormat()));
}

std::error_code lcf::Image::loadFromMemoryEncoded(std::span<const std::byte> data) noexcept
{
    auto expected = load_from_memory_stb(data, m_format);
    if (not expected) { return expected.error(); }
    else { m_image = std::move(*expected); }
    return {};
}

std::error_code lcf::Image::loadFromMemoryPixels(std::span<const std::byte> data, uint32_t width, ImageFormat src_format) noexcept
{
    return this->loadFromMemoryPixels(data, width, src_format, src_format);
}

std::error_code lcf::Image::loadFromMemoryPixels(std::span<const std::byte> data, uint32_t width, ImageFormat src_format, ImageFormat dst_format) noexcept
{
    src_format = enum_decode::decode(src_format);
    size_t row_size_in_bytes = enum_decode::get_channel_count(src_format) * enum_decode::get_bytes_per_channel(src_format) * width;
    if (data.size() % row_size_in_bytes != 0) { return std::make_error_code(std::errc::invalid_argument); }
    uint32_t height = static_cast<uint32_t>(data.size() / row_size_in_bytes);
    dst_format = enum_decode::decode(dst_format);
    m_image = details::generate_image<>(width, height, dst_format);
    m_format = dst_format;
    auto src_view = details::generate_image_view(data, src_format, width);
    details::convert(src_view, details::view(m_image));
    return {};
}

std::error_code Image::saveToFile(const std::filesystem::path &path) const noexcept
{
    ImageFileType file_type = get_file_type(path);
    if (is_stb_save_supported(file_type)) {
        return save_to_file_stb(m_image, m_format, file_type, path);
    }
    //todo: save functions for other file types
    return std::make_error_code(std::errc::invalid_argument);
}

std::span<std::byte> Image::getDataSpan() noexcept
{
    return details::view_as_bytes(m_image);
}

std::span<const std::byte> Image::getDataSpan() const noexcept
{
    return details::view_as_bytes(m_image);
}

//- auxiliary function implementations begin

std::filesystem::path get_extension(ImageFileType type) noexcept
{
    return std::filesystem::path{}.replace_extension(boost::to_lower_copy(std::string{enum_name(type).substr(1)}));
}

ImageFileType get_file_type(const std::filesystem::path & path) noexcept
{
    ImageFileType result = ImageFileType::eInvalid;
    auto ext_str = boost::to_lower_copy(path.extension().string());
    if (ext_str == ".jpg") { return ImageFileType::eJPEG; } //- special case for jpeg
    auto hash_value = hash(ext_str);
    for (auto type : enum_values_v<ImageFileType>) {
        if (hash(get_extension(type).string()) != hash_value) { continue; }
        result = type;
        break;
    }
    return result;
}

ImageInfoData read_info_from_png(const std::filesystem::path & path)
{
    auto info = gil::read_image_info(path.string(), gil::png_tag {})._info;
    ColorSpace color_space = ColorSpace::eInvalid;
    switch (info._color_type) {
        case PNG_COLOR_TYPE_GRAY: { color_space = ColorSpace::eGray; } break;
        case PNG_COLOR_TYPE_GA: { color_space = ColorSpace::eGrayAlpha; } break;
        case PNG_COLOR_TYPE_PALETTE:
        case PNG_COLOR_TYPE_RGB: { color_space = ColorSpace::eRGB; } break;
        case PNG_COLOR_TYPE_RGBA: { color_space = ColorSpace::eRGBA; } break;
        default: break;
    }
    return {
        info._width,
        info._height, 
        enum_decode::get_image_format(color_space, info._bit_depth == 16 ? PixelDataType::eUint16 : PixelDataType::eUint8)
    };
}

ImageInfoData read_info_from_jpeg(const std::filesystem::path &path)
{
    auto info = gil::read_image_info(path.string(), gil::jpeg_tag {})._info;
    ColorSpace color_space = ColorSpace::eInvalid;
    switch (info._color_space) {
        case JCS_GRAYSCALE: { color_space = ColorSpace::eGray; } break;
        case JCS_EXT_RGB:
        case JCS_RGB: { color_space = ColorSpace::eRGB; } break;
        case JCS_YCbCr: { color_space = ColorSpace::eYCbCr; } break;
        case JCS_CMYK: { color_space = ColorSpace::eCMYK; } break;
        case JCS_YCCK: { color_space = ColorSpace::eYCCK; } break;
        case JCS_EXT_RGBX:
        case JCS_EXT_RGBA: { color_space = ColorSpace::eRGBA; } break;
        case JCS_EXT_BGR: { color_space = ColorSpace::eBGR; } break;
        case JCS_EXT_BGRX:
        case JCS_EXT_BGRA: { color_space = ColorSpace::eBGRA; } break;
        case JCS_EXT_XBGR:
        case JCS_EXT_ABGR: { color_space = ColorSpace::eABGR; } break;
        case JCS_EXT_XRGB:
        case JCS_EXT_ARGB: { color_space = ColorSpace::eARGB; } break;
        default: break;
    }
    return {
        info._width,
        info._height,
        enum_decode::get_image_format(color_space, PixelDataType::eUint8), 
    };
}

bool is_stb_load_supported(const ImageInfo & info) noexcept
{
    return enum_decode::is_native_image_format(info.getEncodeFormat());
}

std::expected<ImageVariant, std::error_code> load_from_file_gil(const ImageInfo &info, ImageFormat specific_format) noexcept
{
    std::string path_str = info.getPath().string();
    auto image = details::generate_image<>(info.getWidth(), info.getHeight(), specific_format);
    try {
        switch (info.getFileType()) {
            case ImageFileType::ePNG: { gil::read_image(path_str, image, gil::png_tag {}); } break;
            case ImageFileType::eJPEG: { gil::read_image(path_str, image, gil::jpeg_tag {}); } break;
            case ImageFileType::eBMP: { gil::read_image(path_str, image, gil::bmp_tag {}); } break;
            case ImageFileType::eTGA: { gil::read_image(path_str, image, gil::targa_tag {}); } break;
            default: return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
    } catch (const std::exception & e) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return image;
}

std::expected<ImageVariant, std::error_code> load_from_file_stb(const ImageInfo &info, ImageFormat specific_format) noexcept
{
    std::string path_str = info.getPath().string();
    auto image = details::generate_image<>(info.getWidth(), info.getHeight(), specific_format);
    auto data_span = details::view_as_bytes(image);
    int width, height, channels;
    int requested_channels = enum_decode::get_channel_count(specific_format);
    void * src_data_p = nullptr;
    PixelDataType pixel_data_type = enum_decode::get_pixel_data_type(specific_format);
    switch(pixel_data_type) {
        case PixelDataType::eUint8: {
            src_data_p = stbi_load(path_str.c_str(), &width, &height, &channels, requested_channels);
        } break;
        case PixelDataType::eUint16: {
            src_data_p = stbi_load_16(path_str.c_str(), &width, &height, &channels, requested_channels);
        } break;
        case PixelDataType::eFloat16:
        case PixelDataType::eFloat32: {
            src_data_p = stbi_loadf(path_str.c_str(), &width, &height, &channels, requested_channels);
        }
        default: break;
    }
    if (pixel_data_type == PixelDataType::eFloat16) {
        auto f16_data_span = span_cast<float16_t>(data_span);
        auto src_data_span = std::span(static_cast<const float *>(src_data_p), data_span.size() / sizeof(float));
        std::ranges::copy(src_data_span, f16_data_span.begin());
    } else {
        memcpy(data_span.data(), src_data_p, data_span.size());
    }
    stbi_image_free(src_data_p);
    return image;
}

std::expected<ImageVariant, std::error_code> load_from_memory_stb(std::span<const std::byte> data, ImageFormat &format) noexcept
{
    const stbi_uc * src_data_p = (const stbi_uc *)data.data();
    size_t data_size = data.size();
    bool is_16_bit = stbi_is_16_bit_from_memory(src_data_p, data_size);
    bool is_float = stbi_is_hdr_from_memory(src_data_p, data_size);
    int width, height, channels;
    void * dst_data_p = nullptr;
    if (is_float) {
        dst_data_p = stbi_loadf_from_memory(src_data_p, data_size, &width, &height, &channels, 0);
        data_size = width * height * channels * sizeof(float);
    } else if (is_16_bit) {
        dst_data_p = stbi_load_16_from_memory(src_data_p, data_size, &width, &height, &channels, 0);
        data_size = width * height * channels * sizeof(uint16_t);
    } else {
        dst_data_p = stbi_load_from_memory(src_data_p, data_size, &width, &height, &channels, 0);
        data_size = width * height * channels * sizeof(uint8_t);
    }
    if (not dst_data_p) { return std::unexpected(std::make_error_code(std::errc::invalid_argument)); }
    PixelDataType pixel_data_type = is_float ? PixelDataType::eFloat32 : (is_16_bit ? PixelDataType::eUint16 : PixelDataType::eUint8);
    format = enum_decode::decode_image_format(pixel_data_type, static_cast<uint8_t>(channels));
    auto image = details::generate_image<>(width, height, format);
    auto data_span = details::view_as_bytes(image);
    memcpy(data_span.data(), dst_data_p, data_size);
    stbi_image_free(dst_data_p);
    return image;
}

bool is_stb_save_supported(ImageFileType file_type) noexcept
{
    return file_type == ImageFileType::ePNG or
        file_type == ImageFileType::eJPEG or
        file_type == ImageFileType::eBMP or
        file_type == ImageFileType::eTGA or
        file_type == ImageFileType::eHDR;
}

std::error_code save_to_file_stb(const ImageVariant &image, ImageFormat format, ImageFileType file_type, const std::filesystem::path &path) noexcept
{
    auto bytes = details::view_as_bytes(image);
    auto path_str = path.string();
    bool success = false;
    int width = image.width();
    int height = image.height();
    int channels = enum_decode::get_channel_count(format);
    switch (file_type) {
        case ImageFileType::ePNG: { success = stbi_write_png(path_str.c_str(), width, height, channels, bytes.data(), 0); } break;
        case ImageFileType::eJPEG: { success = stbi_write_jpg(path_str.c_str(), width, height, channels, bytes.data(), 100); } break;
        case ImageFileType::eBMP: { success = stbi_write_bmp(path_str.c_str(), width, height, channels, bytes.data()); } break;
        case ImageFileType::eTGA: { success = stbi_write_tga(path_str.c_str(), width, height, channels, bytes.data()); } break;
        case ImageFileType::eHDR: { success = stbi_write_hdr(path_str.c_str(), width, height, channels, (const float *)(bytes.data())); } break;
        default: break;
    }
    if (success) { return {}; }
    return std::make_error_code(std::errc::io_error);
}
// - auxiliary function implementations end
