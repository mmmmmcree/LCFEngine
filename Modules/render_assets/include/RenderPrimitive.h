#pragma once

#include "Geometry.h"
#include "Material.h"
#include "Matrix.h"
#include <vector>

namespace lcf {
    class RenderPrimitive //readonly, copyable
    {
        using Self = RenderPrimitive;
    public:
        RenderPrimitive() = default;
        ~RenderPrimitive() = default;
        RenderPrimitive(const Self &) = default;
        Self & operator=(const Self &) = default;
        RenderPrimitive(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        Self & setGeometrySharedPtr(const Geometry::SharedPointer & geometry_sp) noexcept { m_geometry_sp = geometry_sp; return *this; }
        Self & setMaterialSharedPtr(const Material::SharedPointer & material_sp) noexcept { m_material_sp = material_sp; return *this; }
        const Geometry & getGeometry() const noexcept { return *m_geometry_sp; }
        const Material & getMaterial() const noexcept { return *m_material_sp; }
    private:
        Geometry::SharedPointer m_geometry_sp;
        Material::SharedPointer m_material_sp;
    };

    using RenderPrimitiveList = std::vector<RenderPrimitive>;
}