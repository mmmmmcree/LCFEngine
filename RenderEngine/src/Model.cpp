#include "Model.h"

using namespace lcf;

Model & Model::addMesh(const Mesh::SharedConstPointer &mesh_scp)
{
    if (not mesh_scp) { return *this; }
    m_mesh_scp_list.emplace_back(mesh_scp);
    return *this;
}

Model & Model::addMaterial(const Material::SharedConstPointer &material_scp)
{
    if (not material_scp) { return *this; }
    m_material_scp_list.emplace_back(material_scp);
    return *this;
}

const Mesh & lcf::Model::getMesh(size_t index) const noexcept
{
    return *m_mesh_scp_list.at(index);
}

const Material & lcf::Model::getMaterial(size_t index) const noexcept
{
    return *m_material_scp_list.at(index);
}
