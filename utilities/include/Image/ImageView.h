#pragma once

#include "Image.h"

namespace lcf {
    class ConstImageView;

    class ImageView
    {
        friend class Image;
        friend class ConstImageView;
    public:
        using NullView = boost::gil::gray8s_view_t;
        using ImageViewVariant = boost::gil::any_image_view<
            NullView,
            
            boost::gil::gray8_view_t,
            boost::gil::gray16_view_t,
            boost::gil::gray32f_view_t,

            boost::gil::rgb8_view_t,
            boost::gil::rgb16_view_t,
            boost::gil::rgb32f_view_t,

            boost::gil::rgba8_view_t,
            boost::gil::rgba16_view_t,
            boost::gil::rgba32f_view_t,

            boost::gil::bgr8_view_t,
            boost::gil::bgra8_view_t,

            boost::gil::argb8_view_t,

            boost::gil::cmyk8_view_t,
            boost::gil::cmyk32f_view_t>;
    public:
        ImageView() = default;
        ImageView(Image & image);
        ImageView(const ImageView & other) = default;
        ImageView & operator=(const ImageView & other) = default;
        ImageView(ImageView && other) = default;
        ImageView & operator=(ImageView && other) = default;
        ImageView(ImageViewVariant && image_view) noexcept : m_image_view(std::move(image_view)) {}
        ImageView & operator=(ImageViewVariant && image_view) noexcept { m_image_view = std::move(image_view); return *this; }
        size_t getWidth() const noexcept;
        size_t getHeight() const noexcept;
        std::pair<size_t, size_t> getDimensions() const noexcept { return { this->getWidth(), this->getHeight() }; }
        size_t getChannelCount() const noexcept;
        size_t getBytesPerChannel() const noexcept;
        size_t getSizeInBytes() const noexcept;
        ImageView getSubView(size_t top_left_x, size_t top_left_y, size_t width, size_t height) const noexcept;
        bool saveTo(const std::filesystem::path & path) const;
    private:
        ImageViewVariant m_image_view = NullView{};
    };
}