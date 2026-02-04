#include "image/details/convert_from_gray_to_color.h"

void lcf::details::convert_from_gray_to_color(ConstGrayImageViewVariant src_view_var, ColorImageViewVariant dst_view_var)
{
    boost::variant2::visit([&](auto && src_view) {
        boost::variant2::visit([&](auto && dst_view) {
            boost::gil::copy_and_convert_pixels(src_view, dst_view);
        }, dst_view_var);
    }, src_view_var);
}