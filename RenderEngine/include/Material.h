#pragma once

#include "Image/Image.h"
#include "PointerDefs.h"
#include <vector>

namespace lcf {
    class Material : public STDPointerDefs<Material>
    {
        using Self = Material;
    public:
        using ImageList  = std::vector<Image::SharedConstPointer>;
        Material() = default;
        Self & setName(std::string_view name) { m_name = name; return *this; }
        const std::string & getName() const noexcept { return m_name; }
        Self & addImage(const Image::SharedConstPointer & image_scp);
        const Image & getImage(size_t index) const noexcept;
    private:
        std::string m_name;
        ImageList m_image_scp_list;
    };
}