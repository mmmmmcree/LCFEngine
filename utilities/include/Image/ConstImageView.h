#pragma once

#include "Image.h"
#include "ImageView.h"

namespace lcf {

    class ConstImageView
    {
        friend class Image;
    public:
        using NullView = boost::gil::gray8sc_view_t;
        using ConstImageViewVariant = boost::gil::any_image_view<
            NullView,
            
            boost::gil::gray8c_view_t,
            boost::gil::gray16c_view_t,
            boost::gil::gray32fc_view_t,
            
            boost::gil::rgb8c_view_t,
            boost::gil::rgb16c_view_t,
            boost::gil::rgb32fc_view_t,
            
            boost::gil::rgba8c_view_t,
            boost::gil::rgba16c_view_t,
            boost::gil::rgba32fc_view_t,
            
            boost::gil::bgr8c_view_t,
            boost::gil::bgra8c_view_t,
            
            boost::gil::argb8c_view_t,
            
            boost::gil::cmyk8c_view_t,
            boost::gil::cmyk32fc_view_t>;
    public:
        ConstImageView() = default;
        ConstImageView(const Image & image);
        ConstImageView(const ImageView & image_view);
        ConstImageView(const ConstImageView & other) = default;
        ConstImageView & operator=(const ConstImageView & other) = default;
        ConstImageView(ConstImageView && other) = default;
        ConstImageView & operator=(ConstImageView && other) = default;
        ConstImageView(ConstImageViewVariant && image_view) noexcept : m_image_view(std::move(image_view)) {}
        ConstImageView & operator=(ConstImageViewVariant && image_view) noexcept { m_image_view = std::move(image_view); return *this; }
        size_t getWidth() const noexcept;
        size_t getHeight() const noexcept;
        std::pair<size_t, size_t> getDimensions() const noexcept { return { this->getWidth(), this->getHeight() }; }
        size_t getChannelCount() const noexcept;
        size_t getBytesPerChannel() const noexcept;
        size_t getSizeInBytes() const noexcept;
        ConstImageView getSubView(size_t top_left_x, size_t top_left_y, size_t width, size_t height) const noexcept;
        bool saveTo(const std::filesystem::path & path) const;
    private:
        bool saveToPNG(const std::filesystem::path & path) const;
        bool saveToJPG(const std::filesystem::path & path) const;
    private:
        ConstImageViewVariant m_image_view = NullView{};
    };   
}