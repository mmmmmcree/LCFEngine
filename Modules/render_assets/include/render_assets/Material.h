#pragma once

#include "render_assets_fwd_decls.h"
#include "render_assets_enums.h"
#include "MaterialParam.h"
#include "enums/enum_value_type.h"
#include "vector_enum_value_types.h"
#include "BufferWriteSegment.h"
#include "bytes.h"
#include "StructureLayout.h"
#include "image/image_fwd_decls.h"
#include "PointerDefs.h"
#include "Vector.h"
#include "ranges/take_from.h"
#include <tsl/robin_map.h>
#include <variant>

namespace lcf {
    class Material : public MaterialPointerDefs
    {
        using Self = Material;
        using ParamMap = tsl::robin_map<MaterialProperty, MaterialParam>;
        using TextureReourceMap = tsl::robin_map<TextureSemantic, ImageSharedPointer>;
    public:
        Material() noexcept = default;
        ~Material() noexcept = default;
        Material(const Self &) = default;
        Self & operator=(const Self &) = default;
        Material(Self &&) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
    public:
        template <MaterialProperty property>
        Self & setParam(const enum_value_t<property> & param) noexcept
        {
            m_params[property] = param;
            return *this;
        }
        Self & setParam(MaterialProperty property, const MaterialParam & param) noexcept;
        template <MaterialProperty property>
        const enum_value_t<property> & getParam() noexcept
        {
            return std::get<enum_value_t<property>>(this->getMaterialParam(property));
        }
        Self & setTextureResource(TextureSemantic semantic, const ImageSharedPointer & texture_resource) noexcept;
        const ImageSharedPointer & getTextureResource(TextureSemantic semantic) const noexcept;
        const MaterialParam & getMaterialParam(MaterialProperty property) const noexcept;
    private:
        ParamMap m_params;
        TextureReourceMap m_texture_resources;
    };

    template <typename Mapping = typename enum_value_type_mapping_traits<VectorType>::type>
    BufferWriteSegments generate_interleaved_segments(const Material & material, ShadingModel shading_model) noexcept
    {
        auto material_properties = enum_values_v<MaterialProperty> | std::views::filter([shading_model](auto property) {
            return contains_flags(enum_decode::get_material_property_flags(shading_model), property);
        });
        StructureLayout layout;
        for (auto property : material_properties) {
            switch (enum_decode::get_vector_type(property)) {
                case VectorType::e1Float32: { layout.addField<enum_value_t<VectorType::e1Float32, Mapping>>(); } break;
                case VectorType::e2Float32: { layout.addField<enum_value_t<VectorType::e2Float32, Mapping>>(); } break;
                case VectorType::e3Float32: { layout.addField<enum_value_t<VectorType::e3Float32, Mapping>>(); } break;
                case VectorType::e4Float32: { layout.addField<enum_value_t<VectorType::e4Float32, Mapping>>(); } break;
                default: break;
            }
        }
        layout.create();
        BufferWriteSegments segments;
        size_t field_index = 0;
        for (auto property : material_properties) {
            auto property_bytes = as_bytes_from_variant(material.getMaterialParam(property));
            segments.add(property_bytes, layout.getFieldOffset(field_index++));
        }
        return segments;
    }
}