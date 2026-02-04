#pragma once

#include "ImageVariant.h"

namespace lcf::details {
    void convert(ConstImageViewVariant src_view_var, ImageViewVariant dst_view_var);

    void convert(ImageViewVariant src_view_var, ImageViewVariant dst_view_var);
}