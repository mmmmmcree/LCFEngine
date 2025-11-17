#include "ConstImageView.h"
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace boost;

lcf::ConstImageView::ConstImageView(const Image &image) : m_image_view(gil::const_view(image.m_image))
{
}

lcf::ConstImageView::ConstImageView(const ImageView &image_view)
{
    variant2::visit([this](auto&& img_view) {
        using ConstViewType = typename std::decay_t<decltype(img_view)>::const_t;
        m_image_view = ConstViewType(img_view);
    }, image_view.m_image_view);
}

size_t lcf::ConstImageView::getWidth() const noexcept
{
    return m_image_view.width();
}

size_t lcf::ConstImageView::getHeight() const noexcept
{
    return m_image_view.height();
}

size_t lcf::ConstImageView::getChannelCount() const noexcept
{
    return m_image_view.num_channels();
}

size_t lcf::ConstImageView::getBytesPerChannel() const noexcept
{
    return variant2::visit([](auto && img_view) {
        using ViewType = std::decay_t<decltype(img_view)>;
        return sizeof(typename gil::channel_type<ViewType>::type);
    }, m_image_view);
}

size_t lcf::ConstImageView::getSizeInBytes() const noexcept
{
    return variant2::visit([](auto && img_view) {
        using ViewType = std::decay_t<decltype(img_view)>;
        return sizeof(typename ViewType::value_type) * img_view.size();
    }, m_image_view);
}

lcf::ConstImageView lcf::ConstImageView::getSubView(size_t top_left_x, size_t top_left_y, size_t width, size_t height) const noexcept
{
    return variant2::visit([top_left_x, top_left_y, width, height](auto && img_view) {
        return ConstImageView(gil::subimage_view(img_view, top_left_x, top_left_y, width, height));
    }, m_image_view);
}

bool lcf::ConstImageView::saveToFile(const std::filesystem::path &path) const
{
    switch (Image::deduceFileType(path)) {
        case Image::FileType::eJPG: { return this->saveToJPG(path); };
        case Image::FileType::ePNG: { return this->saveToPNG(path); };
    }
    return true;
}


bool lcf::ConstImageView::saveToPNG(const std::filesystem::path &path) const
{
    bool successful = variant2::visit([&path](auto&& img_view) -> bool {
        using PixelType = typename std::decay_t<decltype(img_view)>::value_type;
        if constexpr (gil::is_write_supported<PixelType,gil::png_tag>::value) {
            gil::write_view(path.string(), img_view, gil::png_tag{});
            return true;
        }
        return false;
    }, m_image_view);
    return successful;
}

bool lcf::ConstImageView::saveToJPG(const std::filesystem::path &path) const
{
    bool successful = variant2::visit([&path](auto&& img_view) -> bool {
        using PixelType = typename std::decay_t<decltype(img_view)>::value_type;
        if constexpr (gil::is_write_supported<PixelType, gil::jpeg_tag>::value) {
            gil::write_view(path.string(), img_view, gil::jpeg_tag{});
            return true;
        }
        return false;
    }, m_image_view);
    return successful;
}
