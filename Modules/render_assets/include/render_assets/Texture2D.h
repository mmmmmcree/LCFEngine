#pragma once

#include "render_assets_fwd_decls.h"
#include "image/Image.h"

namespace lcf {
    class Texture2D : public Image, public Texture2DPointerDefs
    {
    public:
        using Image::Image;
    };
}