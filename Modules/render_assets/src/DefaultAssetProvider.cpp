#include "render_assets/DefaultAssetProvider.h"
#include "render_assets/constants/geometry_data.h"
#include "image/Image.h"
#include "vector_enum_value_types.h"
#include "Vector.h"
#include "bytes.h"
#include <vector>

using namespace lcf;

DefaultAssetProvider::DefaultAssetProvider()
{
    using Color = Vector4D<uint8_t>;
    std::vector<std::pair<DefaultTexture2DType, Color>> color_map = {
        {DefaultTexture2DType::eWhite,  Color {0xFF, 0xFF, 0xFF, 0xFF}},
        {DefaultTexture2DType::eBlack,  Color {0x00, 0x00, 0x00, 0xFF}},
        {DefaultTexture2DType::eGray,  Color  {0x80, 0x80, 0x80, 0xFF}},
        {DefaultTexture2DType::eRed,    Color {0xFF, 0x00, 0x00, 0xFF}},
        {DefaultTexture2DType::eGreen,  Color {0x00, 0xFF, 0x00, 0xFF}},
        {DefaultTexture2DType::eBlue,  Color {0x00, 0x00, 0xFF, 0xFF}},
    };
    for (const auto & [type, color] : color_map) {
        auto & image_resource = m_texture2d_resources[type] = Image::makeShared();
        image_resource->loadFromMemoryPixels(as_bytes_from_value(color), 1, ImageFormat::eRGBA8Uint);
    }
    //todo Texture2D checkerboard

    m_material_params = {
        {MaterialProperty::eBaseColor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eBaseColor)> {1.0f, 1.0f, 1.0f, 1.0f}}, 
        {MaterialProperty::eRoughness, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eRoughness)> {0.5f}}, 
        {MaterialProperty::eMetallic, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eMetallic)> {0.0f}}, 
        {MaterialProperty::eReflectance, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eReflectance)> {0.5f}}, 
        {MaterialProperty::eAmbientOcclusion, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eAmbientOcclusion)> {1.0f}}, 
        {MaterialProperty::eClearCoat, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eClearCoat)> {0.0f}}, 
        {MaterialProperty::eClearCoatRoughness, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eClearCoatRoughness)> {0.0f}}, 
        {MaterialProperty::eClearCoatNormal, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eClearCoatNormal)> {1.0f}}, 
        {MaterialProperty::eAnisotropy, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eAnisotropy)> {0.0f}}, 
        {MaterialProperty::eAnisotropyDirection, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eAnisotropyDirection)> {1.0f, 0.0f, 0.0f}}, 
        {MaterialProperty::eThickness, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eThickness)> {0.5f}}, 
        {MaterialProperty::eSubsurfacePower, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eSubsurfacePower)> {12.234f}}, 
        {MaterialProperty::eSubsurfaceColor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eSubsurfaceColor)> {1.0f, 1.0f, 1.0f}}, 
        {MaterialProperty::eSheenColor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eSheenColor)> {0.0f, 0.0f, 0.0f}}, 
        {MaterialProperty::eSheenRoughness, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eSheenRoughness)> {0.0f, 0.0f, 0.0f}}, 
        {MaterialProperty::eEmissive, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eEmissive)> {0.0f, 0.0f, 0.0f, 0.0f}}, 
        {MaterialProperty::eNormal, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eNormal)> {0.0f, 0.0f, 1.0f}}, 
        {MaterialProperty::eBentNormal, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eBentNormal)> {0.0f, 0.0f, 1.0f}}, 
        {MaterialProperty::ePostLightingColor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::ePostLightingColor)> {0.0f, 0.0f, 0.0f, 0.0f}}, 
        {MaterialProperty::ePostLightingMixFactor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::ePostLightingMixFactor)> {0.0f}}, 
        {MaterialProperty::eAbsorption, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eAbsorption)> {0.0f, 0.0f, 0.0f}}, 
        {MaterialProperty::eTransmission, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eTransmission)> {0.0f}}, 
        {MaterialProperty::eIOR, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eIOR)> {1.5f}}, 
        {MaterialProperty::eMicroThickness, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eMicroThickness)> {0.0f}}, 
        {MaterialProperty::eSpecularFactor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eSpecularFactor)> {1.0f}}, 
        {MaterialProperty::eSpecularColorFactor, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eSpecularColorFactor)> {1.0f, 1.0f, 1.0f}}, 
        {MaterialProperty::eShadowStrength, enum_value_t<enum_decode::get_vector_type(MaterialProperty::eShadowStrength)> {1.0f}}, 
    };

    m_default_geometry_sp = Geometry::makeShared();

    m_default_geometry_sp->resize(std::size(constants::cube_positions))
        .setAttributes<VertexAttribute::ePosition>(constants::cube_positions)
        .setAttributes<VertexAttribute::eNormal>(constants::cube_normals)
        .setAttributes<VertexAttribute::eTexCoord0>(constants::cube_uvs)
        .setIndices(constants::cube_indices);

    m_default_material_sp = Material::makeShared();
}

const DefaultAssetProvider & DefaultAssetProvider::getInstance() noexcept
{
    static DefaultAssetProvider s_instance;
    return s_instance;
}

const MaterialParam & DefaultAssetProvider::getMaterialParam(MaterialProperty property) const noexcept
{
    return m_material_params.at(property);
}

const Image::SharedPointer & DefaultAssetProvider::getTextureResource(DefaultTexture2DType type) const noexcept
{
    return m_texture2d_resources.at(type);
}

const Image::SharedPointer & DefaultAssetProvider::getTextureResource(TextureSemantic semantic) const noexcept
{
    switch (semantic) {
        case TextureSemantic::eBaseColor: { return this->getTextureResource(DefaultTexture2DType::eWhite); }
        case TextureSemantic::eNormal: { return this->getTextureResource(DefaultTexture2DType::eBlue); }
        default: break;
    }
    return this->getTextureResource(DefaultTexture2DType::eBlack);
}

const Geometry::SharedPointer DefaultAssetProvider::getGeometryResource() const noexcept
{
    return m_default_geometry_sp;
}

const Material::SharedPointer DefaultAssetProvider::getMaterialResource() const noexcept
{
    return m_default_material_sp;
}
