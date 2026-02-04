#pragma once

#include "ImageVariant.h"

namespace lcf::details {
    void convert_from_color_to_color(ConstColorImageViewVariant src_view_var, ColorImageViewVariant dst_view_var);
}