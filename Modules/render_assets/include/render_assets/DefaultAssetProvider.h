#pragma once

#include "render_assets_enums.h"
#include "MaterialParam.h"
#include "Material.h"
#include "Geometry.h"
#include "Image/Image.h"
#include <tsl/robin_map.h>

namespace lcf {
    class DefaultAssetProvider
    {
        using Texture2DResourceMap = tsl::robin_map<DefaultTexture2DType, typename Image::SharedPointer>;
        using MaterialParamMap = tsl::robin_map<MaterialProperty, MaterialParam>;
    public:
        ~DefaultAssetProvider() = default;
        DefaultAssetProvider(const DefaultAssetProvider &) = delete;
        DefaultAssetProvider &operator=(const DefaultAssetProvider &) = delete;
        DefaultAssetProvider(DefaultAssetProvider &&) = delete;
        DefaultAssetProvider &operator=(DefaultAssetProvider &&) = delete;
    public:
        static const DefaultAssetProvider & getInstance() noexcept;
        const MaterialParam & getMaterialParam(MaterialProperty property) const noexcept;
        const Image::SharedPointer & getTextureResource(DefaultTexture2DType type) const noexcept;
        const Image::SharedPointer & getTextureResource(TextureSemantic semantic) const noexcept;
        const Geometry::SharedPointer getGeometryResource() const noexcept;
        const Material::SharedPointer getMaterialResource() const noexcept;
    private:
        DefaultAssetProvider();
    private:
        MaterialParamMap m_material_params;
        Texture2DResourceMap m_texture2d_resources;
        Geometry::SharedPointer m_default_geometry_sp;
        Material::SharedPointer m_default_material_sp;
    };
}