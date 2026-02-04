#include "image/details/convert.h"
#include "image/details/convert_from_color_to_color.h"
#include "image/details/convert_from_color_to_gray.h"
#include "image/details/convert_from_gray_to_color.h"
#include "image/details/convert_from_gray_to_gray.h"

/*
Rationale:
- On MSVC, instantiating a single `convert` that handles all source/destination
  ImageViewVariant combinations in one translation unit causes extremely large
  object files (~200MB), requiring `/bigobj` to compile.
- To avoid the template instantiation explosion, the conversion logic is split
  into four focused units:
  1) convert_from_color_to_color.h
  2) convert_from_color_to_gray.h
  3) convert_from_gray_to_color.cpp
  4) convert_from_gray_to_gray.h
- This keeps each TU small while preserving overall functionality and link-time
  behavior.

Ideal (but problematic on MSVC) approach:
    boost::variant2::visit([&](auto&& src_view) {
        boost::variant2::visit([&](auto&& dst_view) {
            boost::gil::copy_and_convert_pixels(src_view, dst_view);
        }, dst_view_var);
    }, src_view_var);
This direct approach makes the compiler instantiate the full cross product of
view types in a single TU, triggering `/bigobj`.

Implementation note:
- We classify `src_view` and `dst_view` into Color/Gray at compile time using
  traits (is_any_type_of_v) and dispatch to the minimal dedicated function,
  which significantly reduces per-TU code size.
*/

void lcf::details::convert(ConstImageViewVariant src_view_var, ImageViewVariant dst_view_var)
{
    boost::variant2::visit([&](auto && src_view) {
        using src_view_t = std::remove_cvref_t<decltype(src_view)>;
        boost::variant2::visit([&](auto && dst_view) {
            using dst_view_t = std::remove_cvref_t<decltype(dst_view)>;
            constexpr bool is_color_src = is_any_type_of_v<ConstColorImageViewVariant, src_view_t>;
            constexpr bool is_color_dst = is_any_type_of_v<ColorImageViewVariant, dst_view_t>;
            constexpr bool is_gray_src = is_any_type_of_v<ConstGrayImageViewVariant, src_view_t>;
            constexpr bool is_gray_dst = is_any_type_of_v<GrayImageViewVariant, dst_view_t>;
            if constexpr (is_color_src and is_color_dst) {
                convert_from_color_to_color(src_view, dst_view);
            } else if constexpr (is_gray_src and is_gray_dst) {
                convert_from_gray_to_gray(src_view, dst_view);
            } else if constexpr (is_color_src and is_gray_dst) {
                convert_from_color_to_gray(src_view, dst_view);
            } else if constexpr (is_gray_src and is_color_dst) {
                convert_from_gray_to_color(src_view, dst_view);
            }
        }, dst_view_var);
    }, src_view_var);
}

void lcf::details::convert(ImageViewVariant src_view_var, ImageViewVariant dst_view_var)
{
    convert(as_const_view(src_view_var), dst_view_var);
}
