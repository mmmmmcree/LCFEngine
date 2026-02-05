#include "render_assets/Material.h"
#include "render_assets/DefaultAssetProvider.h"
#include "image/Image.h"

using namespace lcf;

Material & Material::setTextureResource(TextureSemantic semantic, const Image::SharedPointer &texture_resource)
{
    m_texture_resources[semantic] = texture_resource;
    return *this;
}

const Image::SharedPointer & Material::getTextureResource(TextureSemantic semantic) const noexcept
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