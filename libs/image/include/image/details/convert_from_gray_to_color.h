#pragma once

#include "ImageVariant.h"

namespace lcf::details {
    void convert_from_gray_to_color(ConstGrayImageViewVariant src_view_var, ColorImageViewVariant dst_view_var);
}