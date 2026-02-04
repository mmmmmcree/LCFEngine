#pragma once

#include "Geometry.h"
#include "Material.h"
#include <vector>

namespace lcf {
    class RenderPrimitive //readonly, copyable
    {
        using Self = RenderPrimitive;
    public:
        RenderPrimitive(); //todo construct with default geometry and material
        ~RenderPrimitive() noexcept = default;
        RenderPrimitive(const Self &) = default;
        Self & operator=(const Self &) = default;
        RenderPrimitive(Self &&) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
    public:
        Self & setGeometryResource(const Geometry::SharedPointer & geometry_sp) noexcept { m_geometry_sp = geometry_sp; return *this; }
        Self & setMaterialResource(const Material::SharedPointer & material_sp) noexcept { m_material_sp = material_sp; return *this; }
        const Geometry::SharedPointer & getGeometryResource() const noexcept { return m_geometry_sp; }
        const Material::SharedPointer & getMaterialResource() const noexcept { return m_material_sp; }
        const Geometry & getGeometry() const noexcept { return *m_geometry_sp; }
        const Material & getMaterial() const noexcept { return *m_material_sp; }
    private:
        Geometry::SharedPointer m_geometry_sp;
        Material::SharedPointer m_material_sp;
    };

    using RenderPrimitiveList = std::vector<RenderPrimitive>;
}