#include "ImageView.h"
#include "ConstImageView.h"

using namespace boost;

lcf::ImageView::ImageView(Image &image) : m_image_view(gil::view(image.m_image))
{
}

size_t lcf::ImageView::getWidth() const noexcept
{
    return m_image_view.width();
}

size_t lcf::ImageView::getHeight() const noexcept
{
    return m_image_view.height();
}

size_t lcf::ImageView::getChannelCount() const noexcept
{
    return m_image_view.num_channels();
}

size_t lcf::ImageView::getBytesPerChannel() const noexcept
{
    return variant2::visit([](auto && img_view) {
        using ViewType = std::decay_t<decltype(img_view)>;
        return sizeof(typename gil::channel_type<ViewType>::type);
    }, m_image_view);
}

size_t lcf::ImageView::getSizeInBytes() const noexcept
{
    return variant2::visit([](auto && img_view) {
        using ViewType = std::decay_t<decltype(img_view)>;
        return sizeof(typename ViewType::value_type) * img_view.size();
    }, m_image_view);
}

lcf::ImageView lcf::ImageView::getSubView(size_t top_left_x, size_t top_left_y, size_t width, size_t height) const noexcept
{
    return variant2::visit([top_left_x, top_left_y, width, height](auto && img_view) {
        return ImageView(gil::subimage_view(img_view, top_left_x, top_left_y, width, height));
    }, m_image_view);
}

bool lcf::ImageView::saveToFile(const std::filesystem::path &path) const
{
    return ConstImageView(*this).saveToFile(path);
}
