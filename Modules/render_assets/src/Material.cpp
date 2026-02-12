#include "render_assets/Material.h"
#include "render_assets/DefaultAssetProvider.h"
#include "image/Image.h"
#include "bytes.h"

using namespace lcf;

Material & Material::setTextureResource(TextureSemantic semantic, const Image::SharedPointer &texture_resource) noexcept
{
    m_texture_resources[semantic] = texture_resource;
    return *this;
}

Material & Material::setParam(MaterialProperty property, const MaterialParam & param) noexcept
{
    auto src_bytes = as_bytes_from_variant(param);
    Vector4D<float> dst_value;
    std::ranges::copy(src_bytes, as_bytes_from_value(dst_value).begin());
    switch (enum_decode::get_vector_type(property)) {
        case VectorType::e1Float32: { m_params[property] = dst_value.x; } break;
        case VectorType::e2Float32: { m_params[property] = dst_value.toVector2D(); } break;
        case VectorType::e3Float32: { m_params[property] = dst_value.toVector3D(); } break;
        case VectorType::e4Float32: { m_params[property] = dst_value; } break;
        default: break;
    }
    return *this;
}

const Image::SharedPointer &Material::getTextureResource(TextureSemantic semantic) const noexcept
{
    auto it = m_texture_resources.find(semantic);
    if (it != m_texture_resources.end()) { return it->second; }
    return DefaultAssetProvider::getInstance().getTextureResource(semantic);
}

const MaterialParam & Material::getMaterialParam(MaterialProperty property) const noexcept
{
    auto it = m_params.find(property);
    if (it != m_params.end()) { return it->second; }
    return DefaultAssetProvider::getInstance().getMaterialParam(property);
}