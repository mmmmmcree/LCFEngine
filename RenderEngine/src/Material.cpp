#include "Material.h"

using namespace lcf;

Material & Material::addImage(const Image::SharedConstPointer &image_scp)
{
    m_image_scp_list.emplace_back(image_scp);
    return *this;
}

const Image & Material::getImage(size_t index) const noexcept
{
    return *m_image_scp_list.at(index);
}
