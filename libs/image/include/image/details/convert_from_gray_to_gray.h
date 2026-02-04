#pragma once

#include "ImageVariant.h"

namespace lcf::details {
    void convert_from_gray_to_gray(ConstGrayImageViewVariant src_view_var, GrayImageViewVariant dst_view_var);
}