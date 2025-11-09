#pragma once

#include "Material.h"
#include "Mesh.h"

namespace lcf {
    class Model
    {
        using Self = Model;
    public:
        using MeshList = std::vector<Mesh::SharedConstPointer>;
        using MaterialList = std::vector<Material::SharedConstPointer>;
        Model() = default;
        Self & addMesh(const Mesh::SharedConstPointer & mesh_scp);
        Self & addMaterial(const Material::SharedConstPointer & material_scp);
        const Mesh & getMesh(size_t index) const noexcept;
        const Material & getMaterial(size_t index) const noexcept;
    private:
        MeshList m_mesh_scp_list;
        MaterialList m_material_scp_list;
    };
}