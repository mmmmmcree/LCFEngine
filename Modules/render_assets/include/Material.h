#pragma once

#include "Image/Image.h"
#include <vector>
#include "PointerDefs.h"

namespace lcf {
    class Material : public STDPointerDefs<Material>
    {
        using Self = Material;
        using TextureSharedPtrList = std::vector<typename Image::SharedPointer>;
    public:
        Material() = default;
        ~Material() = default;
        Material(const Self &) = default;
        Self & operator=(const Self &) = default;
        Material(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
    private:
        TextureSharedPtrList m_image_sp_list;
    };
}