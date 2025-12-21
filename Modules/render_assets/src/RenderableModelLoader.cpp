#include "RenderableModelLoader.h"
#include "log.h"
#include "bytes.h"
#include "Transform.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Image/Image.h"
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <algorithm>

using namespace lcf;
namespace stdv = std::views;
namespace stdr = std::ranges;

using AssimpHierarchyNode = std::pair<size_t, const aiNode *>;

Matrix4x4 to_matrix4x4(const aiMatrix4x4 & ai_mat);

RenderableModel::HierarchyNode to_hierarchy_node(const AssimpHierarchyNode &ai_hierarchy_node);

void process_mesh(Geometry & geometry, const aiMesh & ai_mesh);

struct RenderableModelLoader::Impl
{
    Impl() = default;
    ~Impl() = default;
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl(Impl &&) = default;
    Impl &operator=(Impl &&) = default;

    Assimp::Importer m_importer;
    const aiScene * m_ai_scene = nullptr;
    std::filesystem::path m_asset_path;
    std::vector<Geometry::SharedPointer> m_geometry_sp_list;
    std::vector<Material::SharedPointer> m_material_sp_list;
};

lcf::RenderableModelLoader::RenderableModelLoader() :
    m_impl_up(std::make_unique<Impl>())
{
}

lcf::RenderableModelLoader::~RenderableModelLoader()
{
}

std::optional<RenderableModel> RenderableModelLoader::load(const std::filesystem::path &path) const noexcept
{
    auto expected_model = this->analyze(path);
    if (not expected_model) {
        lcf_log_error("Failed to analyze model: {}", expected_model.error().message());
        return std::nullopt;
    }
    auto & model = expected_model.value();
    this->processGeometries(model);
    this->processMaterials(model);
    return std::move(model);
}

std::expected<RenderableModel, std::error_code> RenderableModelLoader::analyze(const std::filesystem::path &path) const noexcept
{
    if (not std::filesystem::exists(path)) {
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }
    const aiScene * ai_scene = m_impl_up->m_importer.ReadFile(
        path.string().c_str(),
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes
    );
    if (not ai_scene) {
        lcf_log_error(m_impl_up->m_importer.GetErrorString());
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    m_impl_up->m_ai_scene = ai_scene;
    m_impl_up->m_asset_path = path;
    RenderableModel model;
    this->buildModel(model);
    return model;
}

void RenderableModelLoader::processGeometries(RenderableModel &model) const noexcept
{
    const aiScene* ai_scene = m_impl_up->m_ai_scene;
    auto& geometry_sp_list = m_impl_up->m_geometry_sp_list;
    for (size_t i = 0; i < ai_scene->mNumMeshes; ++i) {
        process_mesh(*geometry_sp_list[i], *ai_scene->mMeshes[i]);
    }
}

void RenderableModelLoader::processMaterials(RenderableModel &model) const noexcept
{
}

void lcf::RenderableModelLoader::buildModel(RenderableModel &model) const noexcept
{
    const aiScene * ai_scene = m_impl_up->m_ai_scene;
    std::queue<AssimpHierarchyNode> hierarchy_node_queue;
    hierarchy_node_queue.emplace(-1, ai_scene->mRootNode);
    std::vector<AssimpHierarchyNode> hierarchy_node_list;
    while (not hierarchy_node_queue.empty()) {
        auto [parent_index, ai_node] = hierarchy_node_queue.front();
        hierarchy_node_queue.pop();
        hierarchy_node_list.emplace_back(parent_index, ai_node);
        parent_index = hierarchy_node_list.size() - 1;
        for (uint32_t i = 0; i < ai_node->mNumChildren; ++i) {
            hierarchy_node_queue.emplace(parent_index, ai_node->mChildren[i]);
        }
    }
    model.m_hierarchy_node_list.assign_range(hierarchy_node_list | stdv::transform(to_hierarchy_node));
   
    size_t geometry_num = ai_scene->mNumMeshes;
    size_t material_num = ai_scene->mNumMaterials;
    auto & geometry_sp_list = m_impl_up->m_geometry_sp_list;
    auto & material_sp_list = m_impl_up->m_material_sp_list;
    geometry_sp_list.resize(geometry_num);
    material_sp_list.resize(material_num);
    stdr::for_each(geometry_sp_list, [](auto & geometry_sp) { geometry_sp = Geometry::makeShared(); });
    stdr::for_each(material_sp_list, [](auto & material_sp) { material_sp = Material::makeShared(); });
    auto & render_primitive_list = model.m_render_primitive_list;
    render_primitive_list.resize(geometry_num);
    for (auto [index, render_primitive] : render_primitive_list | std::views::enumerate) {
        render_primitive.setGeometrySharedPtr(geometry_sp_list[index]);
        render_primitive.setMaterialSharedPtr(material_sp_list[index]);
    }
}

Matrix4x4 to_matrix4x4(const aiMatrix4x4 & ai_mat)
{
    return {
        ai_mat.a1, ai_mat.a2, ai_mat.a3, ai_mat.a4,
        ai_mat.b1, ai_mat.b2, ai_mat.b3, ai_mat.b4,
        ai_mat.c1, ai_mat.c2, ai_mat.c3, ai_mat.c4,
        ai_mat.d1, ai_mat.d2, ai_mat.d3, ai_mat.d4
    };
}

RenderableModel::HierarchyNode to_hierarchy_node(const AssimpHierarchyNode &ai_hierarchy_node)
{
    RenderableModel::HierarchyNode hierarchy_node;
    auto [parent_index, ai_node] = ai_hierarchy_node;
    hierarchy_node.m_parent_index = parent_index;
    hierarchy_node.m_primitive_indices.assign_range(std::span(ai_node->mMeshes, ai_node->mNumMeshes));
    hierarchy_node.m_local_matrix = to_matrix4x4(ai_node->mTransformation);
    return hierarchy_node;
}

void process_mesh(Geometry &geometry, const aiMesh & ai_mesh)
{
    size_t vertex_num = ai_mesh.mNumVertices;
    geometry.create(vertex_num);
    std::vector<uint32_t> indices;
    for (const auto & face : std::span(ai_mesh.mFaces, ai_mesh.mNumFaces)) {
        geometry.addFace({static_cast<uint32_t>(indices.size()), static_cast<uint32_t>(face.mNumIndices)});
        indices.append_range(std::span(face.mIndices, face.mNumIndices));
    }
    geometry.setIndices(std::move(indices))
        .setVertexAttributes(VertexSemantic::ePosition, as_bytes_from_ptr(ai_mesh.mVertices, vertex_num))
        .setVertexAttributes(VertexSemantic::eNormal, as_bytes_from_ptr(ai_mesh.mNormals, vertex_num));
    if (ai_mesh.HasTextureCoords(0)) {
        geometry.setVertexAttributes(VertexSemantic::eTexCoord0, as_bytes_from_ptr(ai_mesh.mTextureCoords[0], vertex_num));
    }
}

class AssetManager
{
/*
    <id, Image::SharedPointer> texture_map;
    <id, Geometry::SharedPointer> geometry_map;
*/
};
class BackendAssetManager
{
/*
    <Image::WeakPointer, BackendTexture::SharedPointer> texture_map;
    <Geometry::WeakPointer, BackendGeometry::SharedPointer> geometry_map;
*/
};

