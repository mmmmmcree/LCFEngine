#include "Image.h"
#include "ImageView.h"
#include "ConstImageView.h"
#include "details/convert_from_gray_to_gray.h"
#include "details/convert_from_color_to_color.h"
#include "details/convert_from_gray_to_color.h"
#include "details/convert_from_color_to_gray.h"
#include <unordered_map>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/io/jpeg.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace lcf;
using namespace boost;

uint32_t lcf::Image::getWidth() const noexcept
{
    return m_image.width();
}

uint32_t lcf::Image::getHeight() const noexcept
{
    return m_image.height();
}

ImageView lcf::Image::getImageView() noexcept
{
    return ImageView(*this);
}

ConstImageView lcf::Image::getImageView() const noexcept
{
    return ConstImageView(*this);
}

std::span<std::byte> lcf::Image::getInterleavedDataSpan() noexcept
{
    return variant2::visit([this](auto && img_view) {
        using PixelType = typename std::remove_cvref_t<decltype(img_view)>::value_type;
        auto data = reinterpret_cast<std::byte *>(gil::interleaved_view_get_raw_data(img_view));
        return std::span<std::byte>(data, sizeof(PixelType) * img_view.size());
    }, gil::view(m_image));
}

std::span<const std::byte> lcf::Image::getInterleavedDataSpan() const noexcept
{
    return boost::variant2::visit([this](auto && img_view) {
        using PixelType = typename std::remove_cvref_t<decltype(img_view)>::value_type;
        auto data = reinterpret_cast<const std::byte *>(boost::gil::interleaved_view_get_raw_data(img_view));
        return std::span<const std::byte>(data, sizeof(PixelType) * img_view.size());
    }, gil::const_view(m_image));
}

bool lcf::Image::loadFromFile(const std::filesystem::path &path)
{
    if (not std::filesystem::exists(path)) { return false; }
    bool successful = false;
    switch (Image::deduceFileType(path)) {
        case Image::FileType::eJPG: { successful = this->loadFromJPG(path); } break;
        case Image::FileType::ePNG: { successful = this->loadFromPNG(path); } break;
    }
    return successful;
}

bool lcf::Image::loadFromMemory(std::span<const std::byte> data, Format format, size_t width)
{
    size_t row_size_in_bytes = getChannelCount(format) * getBytesPerChannel(format) * width;
    if (data.size() % row_size_in_bytes != 0) { return false; }
    size_t height = data.size() / row_size_in_bytes;
    switch (format) {
        case Format::eGray8Uint: { m_image = gil::gray8_image_t(width, height); } break;
        case Format::eGray16Uint: { m_image = gil::gray16_image_t(width, height); } break;
        case Format::eGray32Float: { m_image = gil::gray32f_image_t(width, height); } break;
        case Format::eRGB8Uint: { m_image = gil::rgb8_image_t(width, height); } break;
        case Format::eRGB16Uint: { m_image = gil::rgb16_image_t(width, height); } break;
        case Format::eRGB32Float: { m_image = gil::rgb32f_image_t(width, height); } break;
        case Format::eBGR8Uint: { m_image = gil::bgr8_image_t(width, height); } break;
        case Format::eRGBA8Uint: { m_image = gil::rgba8_image_t(width, height); } break;
        case Format::eRGBA16Uint: { m_image = gil::rgba16_image_t(width, height); } break;
        case Format::eRGBA32Float: { m_image = gil::rgba32f_image_t(width, height); } break;
        case Format::eBGRA8Uint: { m_image = gil::bgra8_image_t(width, height); } break;
        case Format::eARGB8Uint: { m_image = gil::argb8_image_t(width, height); } break;
        case Format::eCMYK8Uint: { m_image = gil::cmyk8_image_t(width, height); } break;
        case Format::eCMYK32Float: { m_image = gil::cmyk32f_image_t(width, height); } break;
    }
    m_format = format;
    if (m_format != Format::eInvalid) {
        std::memcpy(this->getInterleavedDataSpan().data(), data.data(), data.size());
    }
    return true;
}

bool lcf::Image::saveToFile(const std::filesystem::path &path) const
{
    return this->getImageView().saveToFile(path);
}

Image & lcf::Image::recreate(size_t width, size_t height, size_t alignment)
{
    m_image.recreate(width, height, alignment);
    return *this;
}

Image & lcf::Image::convertTo(Format format)
{
    if (m_format == format) { return *this; }

    if (getColorSpace(m_format) == ColorSpace::eGray and getColorSpace(format) == ColorSpace::eGray) {
        details::convert_from_gray_to_gray(m_image, m_format, format);
    } else if (getColorSpace(m_format) == ColorSpace::eGray) {
        details::convert_from_gray_to_color(m_image, m_format, format);
    } else if (getColorSpace(format) == ColorSpace::eGray) {
        details::convert_from_color_to_gray(m_image, m_format, format);
    } else {
        details::convert_from_color_to_color(m_image, m_format, format);
    }
    m_format = format;
    return *this;
}

Image & lcf::Image::flipUpDown()
{
    boost::gil::apply_operation(m_image, [](auto& img) {
        auto v = gil::view(img);
        size_t height = v.height();
        for (size_t y = 0; y < height / 2; ++y) {
            auto top_row = v.row_begin(y);
            auto bottom_row = v.row_begin(height - 1 - y);
            std::swap_ranges(top_row, v.row_end(y), bottom_row);
        }
    });
    return *this;
}

Image & lcf::Image::flipLeftRight()
{
    boost::gil::apply_operation(m_image, [](auto& img) {
        auto v = gil::view(img);
        size_t height = v.height();
        for (size_t y = 0; y < height; ++y) {
            std::reverse(v.row_begin(y), v.row_end(y));
        }
    });
    return *this;
}

std::filesystem::path lcf::Image::getExtension(FileType type)
{
    std::filesystem::path ext;
    switch (type)
    {
        case FileType::ePNG: { ext = ".png"; } break; 
        case FileType::eJPG: { ext = ".jpg"; } break;
        case FileType::eHDR: { ext = ".hdr"; } break;
    }
    return ext;
}

Image::FileType lcf::Image::deduceFileType(const std::filesystem::path & path)
{
    static std::unordered_map<std::filesystem::path, FileType> file_type_map
    {
        {".png", FileType::ePNG},
        {".jpg", FileType::eJPG},
        {".jpeg", FileType::eJPG},
        {".hdr", FileType::eHDR},
    };
    auto ext = path.extension();
    if (not path.has_extension() or not file_type_map.contains(ext)) { return FileType::eInvalid; }
    return file_type_map.at(ext);
}

bool lcf::Image::loadFromPNG(const std::filesystem::path & path)
{
    if (not std::filesystem::exists(path)) { return false; }
    gil::read_image(path.string(), m_image, gil::png_tag{});
    this->updateFormat();
    return m_format != Format::eInvalid;
}

bool lcf::Image::loadFromJPG(const std::filesystem::path & path)
{
    if (not std::filesystem::exists(path)) { return false; }
    gil::read_image(path.string(), m_image, gil::jpeg_tag{});
    this->updateFormat();
    return m_format != Format::eInvalid;
}

void lcf::Image::updateFormat()
{
    if (m_image.width() == 0 or m_image.height() == 0) {
        m_format = Format::eInvalid;
        return;
    }
    variant2::visit([this](auto && img_view) {
        using PixelType = typename std::remove_cvref_t<decltype(img_view)>::value_type;
        if constexpr (std::is_same_v<PixelType, NullImage::value_type>) { m_format = Format::eInvalid; }
        else if constexpr (std::is_same_v<PixelType, gil::gray8_pixel_t>) { m_format = Format::eGray8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::gray16_pixel_t>) { m_format = Format::eGray16Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::gray32f_pixel_t>) { m_format = Format::eGray32Float; }
        else if constexpr (std::is_same_v<PixelType, gil::rgb8_pixel_t>) { m_format = Format::eRGB8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::rgb16_pixel_t>) { m_format = Format::eRGB16Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::rgb32f_pixel_t>) { m_format = Format::eRGB32Float; }
        else if constexpr (std::is_same_v<PixelType, gil::bgr8_pixel_t>) { m_format = Format::eBGR8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::rgba8_pixel_t>) { m_format = Format::eRGBA8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::rgba16_pixel_t>) { m_format = Format::eRGBA16Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::rgba32f_pixel_t>) { m_format = Format::eRGBA32Float; }
        else if constexpr (std::is_same_v<PixelType, gil::bgra8_pixel_t>) { m_format = Format::eBGRA8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::argb8_pixel_t>) { m_format = Format::eARGB8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::cmyk8_pixel_t>) { m_format = Format::eCMYK8Uint; }
        else if constexpr (std::is_same_v<PixelType, gil::cmyk32f_pixel_t>) { m_format = Format::eCMYK32Float; }
        else { m_format = Format::eInvalid; }
    }, gil::view(m_image));
}
