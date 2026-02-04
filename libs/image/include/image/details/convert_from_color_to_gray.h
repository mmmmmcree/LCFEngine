#pragma once

#include "ImageVariant.h"

namespace lcf::details {
    void convert_from_color_to_gray(ConstColorImageViewVariant src_view_var, GrayImageViewVariant dst_view_var);
}
