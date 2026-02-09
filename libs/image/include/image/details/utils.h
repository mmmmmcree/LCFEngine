#pragma once

#include "ImageVariant.h"
#include "../image_enums.h"
#include <boost/mp11.hpp>
#include <span>

namespace lcf::details {
    template <typename Image>
    auto view(Image && image) noexcept
    {
        if constexpr (std::is_const_v<std::remove_reference_t<Image>>) {
            return boost::gil::const_view(image);
        } else {
            return boost::gil::view(image);
        }
    }

    template <typename Loc>
    inline auto as_const_view(const boost::gil::image_view<Loc> & view) noexcept
    {
        return typename boost::gil::image_view<Loc>::const_t {view};
    }

    template <typename ...Views>
    inline auto as_const_view(const boost::gil::any_image_view<Views...> & view) noexcept
    {
        using ConstView = boost::gil::any_image_view<typename Views::const_t...>;
        return boost::variant2::visit([](auto && member) -> ConstView {
            return ConstView{as_const_view(member)};
        }, view);
    }

    template <typename ImageVar>
    auto view_as_bytes(ImageVar && image_var) noexcept
    {
        constexpr bool is_const = std::is_const_v<std::remove_reference_t<ImageVar>>;
        using byte_t = std::conditional_t<is_const, const std::byte, std::byte>;
        return boost::variant2::visit([](auto && img_view) {
            using PixelType = typename std::remove_cvref_t<decltype(img_view)>::value_type;
            auto data = reinterpret_cast<byte_t *>(boost::gil::interleaved_view_get_raw_data(img_view));
            return std::span<byte_t>(data, sizeof(PixelType) * img_view.size());
        }, view(image_var));
    }

    template <typename Any, typename T>
    struct is_any_type_of;

    template <typename ...AnyTypes, typename T>
    struct is_any_type_of<boost::gil::any_image<AnyTypes...>, T> 
        : boost::mp11::mp_contains<boost::mp11::mp_list<AnyTypes...>, T> {};

    template <typename ...AnyTypes, typename T>
    struct is_any_type_of<boost::gil::any_image_view<AnyTypes...>, T>
        : boost::mp11::mp_contains<boost::mp11::mp_list<AnyTypes...>, T> {};

    template <typename Any, typename T>
    inline constexpr bool is_any_type_of_v = is_any_type_of<Any, T>::value;

    template <typename DstImageVar, typename SrcImageVar>
    auto view_as(SrcImageVar && image_var) noexcept
    {
        using result_t = std::conditional_t<
            std::is_const_v<std::remove_reference_t<SrcImageVar>>,
            decltype(boost::gil::const_view(std::declval<const DstImageVar &>())),
            decltype(boost::gil::view(std::declval<DstImageVar &>()))
        >;
        return boost::variant2::visit([](auto && src_img) -> result_t {
            using T = std::remove_cvref_t<decltype(src_img)>;
            if constexpr (is_any_type_of_v<DstImageVar, T>) { return view(src_img);}
            else { return result_t {}; }
        }, std::forward<SrcImageVar>(image_var));
    }

    template <typename ImageVar>
    auto view_as_colored(ImageVar && image_var) noexcept
    {
        return view_as<ColorImageVariant>(std::forward<ImageVar>(image_var));
    }

    template <typename ImageVar>
    auto view_as_gray(ImageVar && image_var) noexcept
    {
        return view_as<GrayImageVariant>(std::forward<ImageVar>(image_var));
    }

    template <typename Allocator = std::allocator<unsigned char>>
    ImageVariant<Allocator> generate_image(uint32_t width, uint32_t height, ImageFormat format)
    {
        switch (format) {
            case ImageFormat::eGray8Uint: { return gray8_image_t<Allocator>(width, height); }
            case ImageFormat::eGray16Uint: { return gray16_image_t<Allocator>(width, height); }
            case ImageFormat::eGray16Float: { return gray16f_image_t<Allocator>(width, height); }
            case ImageFormat::eGray32Float: { return gray32f_image_t<Allocator>(width, height); }
            case ImageFormat::eGrayAlpha8Uint: { return gray_alpha8_image_t<Allocator>(width, height); }
            case ImageFormat::eGrayAlpha16Uint: { return gray_alpha16_image_t<Allocator>(width, height); }
            case ImageFormat::eGrayAlpha16Float: { return gray_alpha16f_image_t<Allocator>(width, height); }
            case ImageFormat::eGrayAlpha32Float: { return gray_alpha32f_image_t<Allocator>(width, height); }
            case ImageFormat::eRGB8Uint: { return rgb8_image_t<Allocator>(width, height); }
            case ImageFormat::eRGB16Uint: { return rgb16_image_t<Allocator>(width, height); }
            case ImageFormat::eRGB16Float: { return rgb16f_image_t<Allocator>(width, height); }
            case ImageFormat::eRGB32Float: { return rgb32f_image_t<Allocator>(width, height); }
            case ImageFormat::eRGBA8Uint: { return rgba8_image_t<Allocator>(width, height); }    
            case ImageFormat::eRGBA16Uint: { return rgba16_image_t<Allocator>(width, height); }
            case ImageFormat::eRGBA16Float: { return rgba16f_image_t<Allocator>(width, height); }
            case ImageFormat::eRGBA32Float: { return rgba32f_image_t<Allocator>(width, height); }
            default: break;
        }
        return {};
    }

    inline ConstImageViewVariant generate_image_view(std::span<const std::byte> data, ImageFormat format, uint32_t width) noexcept
    {
        size_t row_size_in_bytes = enum_decode::get_channel_count(format) * enum_decode::get_bytes_per_channel(format) * width;
        uint32_t height = static_cast<uint32_t>(data.size() / row_size_in_bytes);
        switch (format) {
            case ImageFormat::eGray8Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray8_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGray16Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray16_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGray16Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray16f_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGray32Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray32f_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGrayAlpha8Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray_alpha8_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGrayAlpha16Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray_alpha16_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGrayAlpha16Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray_alpha16f_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eGrayAlpha32Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::gray_alpha32f_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGB8Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgb8_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGB16Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgb16_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGB16Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgb16f_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGB32Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgb32f_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGBA8Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgba8_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGBA16Uint: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgba16_pixel_t*>(data.data()), row_size_in_bytes); }
            case ImageFormat::eRGBA16Float: { return boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::rgba16f_pixel_t*>(data.data()), row_size_in_bytes); }
            default: break;
        }
        return {};
    }
}