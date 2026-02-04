#include "render_assets/ModelLoader.h"
#include "enums/enum_cast.h"
#include "log.h"
#include "bytes.h"
#include "Transform.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Image/Image.h"
#include <unordered_map>
#include <queue>
#include <algorithm>

using namespace lcf;
namespace stdv = std::views;
namespace stdr = std::ranges;

template <> inline constexpr bool is_pointer_type_convertible_v<aiVector3D, Vector3D<float>> = true;

using AssimpHierarchyNode = std::pair<size_t, const aiNode *>;

Matrix4x4 to_matrix4x4(const aiMatrix4x4 & ai_mat);


template <>
struct enum_mapping_traits<TextureSemantic, aiTextureType>
{
    static constexpr std::tuple<TextureSemantic, aiTextureType> mappings[] = {
        { TextureSemantic::eBaseColor, aiTextureType_BASE_COLOR },
        { TextureSemantic::eMetallicRoughness, aiTextureType_GLTF_METALLIC_ROUGHNESS },
        { TextureSemantic::eNormal, aiTextureType_NORMALS },
        { TextureSemantic::eOcclusion, aiTextureType_AMBIENT_OCCLUSION },
        { TextureSemantic::eEmissive, aiTextureType_EMISSIVE },
        { TextureSemantic::eHeight, aiTextureType_HEIGHT },
        { TextureSemantic::eClearCoat, aiTextureType_CLEARCOAT },
        { TextureSemantic::eClearCoatRoughness, aiTextureType_UNKNOWN },
        { TextureSemantic::eClearCoatNormal, aiTextureType_UNKNOWN },
        { TextureSemantic::eSheenColor, aiTextureType_SHEEN },
        { TextureSemantic::eSheenRoughness, aiTextureType_UNKNOWN },
        { TextureSemantic::eVolumeThickness, aiTextureType_OPACITY },
        { TextureSemantic::eTransmission, aiTextureType_TRANSMISSION },
        { TextureSemantic::eSpecularColor, aiTextureType_MAYA_SPECULAR_COLOR },
        { TextureSemantic::eSpecular, aiTextureType_MAYA_SPECULAR }
    };
};

std::tuple<std::span<const std::byte>, ImageFormat, uint32_t> interpret_ai_texture(const aiTexture & ai_texture);

Model::HierarchyNode to_hierarchy_node(const AssimpHierarchyNode & ai_hierarchy_node);

void process_mesh(Geometry & geometry, const aiMesh & ai_mesh);

void process_material(Material & material, const aiMaterial & ai_material);

struct ModelLoader::Impl
{
    Impl() = default;
    ~Impl() = default;
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl(Impl &&) = default;
    Impl &operator=(Impl &&) = default;

    std::optional<Model> load(const std::filesystem::path & path) noexcept;
    std::expected<Model, std::error_code> analyze(const std::filesystem::path & path) noexcept;
    std::optional<std::error_code> loadTextures() noexcept;
    void processGeometries(Model & model) noexcept;
    void processMaterials(Model & model) noexcept;
    void buildModel(Model & model) noexcept;

    Assimp::Importer m_importer;
    const aiScene * m_ai_scene_p = nullptr;
    std::filesystem::path m_asset_directory_path;
    
    std::vector<Geometry::SharedPointer> m_geometry_resource_list;
    std::vector<Material::SharedPointer> m_material_resource_list;
};

ModelLoader::ModelLoader() :
    m_impl_up(std::make_unique<Impl>())
{
}

ModelLoader::~ModelLoader()
{
}

std::optional<Model> ModelLoader::load(const std::filesystem::path &path) const noexcept
{
    return m_impl_up->load(path);
}

std::optional<Model> ModelLoader::Impl::load(const std::filesystem::path &path) noexcept
{
    auto expected_model = this->analyze(path);
    if (not expected_model) {
        lcf_log_error("Failed to analyze model: {}", expected_model.error().message());
        return std::nullopt;
    }
    auto error_code_opt = this->loadTextures();
    if (error_code_opt) {
        lcf_log_error("Failed to load textures: {}", error_code_opt.value().message());
        return std::nullopt;
    }
    auto & model = expected_model.value();
    this->processGeometries(model);
    this->processMaterials(model);
    return std::move(model);
}

std::expected<Model, std::error_code> ModelLoader::Impl::analyze(const std::filesystem::path &path) noexcept
{
    if (not std::filesystem::exists(path)) {
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    } else if (not std::filesystem::is_regular_file(path)) {
        return std::unexpected(std::make_error_code(std::errc::is_a_directory));
    }
    const aiScene * ai_scene = m_importer.ReadFile(
        path.string().c_str(),
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes
    );
    if (not ai_scene) {
        lcf_log_error(m_importer.GetErrorString());
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    m_ai_scene_p = ai_scene;
    m_asset_directory_path = path.parent_path();
    Model model;
    this->buildModel(model);
    return model;
}

std::optional<std::error_code> ModelLoader::Impl::loadTextures() noexcept
{
    std::unordered_map<std::string, Image::SharedPointer> texture_resource_map;
    for (uint32_t i = 0; i < m_ai_scene_p->mNumMaterials; ++i) {
        const aiMaterial & ai_material = *m_ai_scene_p->mMaterials[i];
        Material & material = *m_material_resource_list[i];
        for (auto texture_semantic : enum_values_v<TextureSemantic>) {
            aiString ai_path_str;
            auto result = ai_material.GetTexture(enum_cast<aiTextureType>(texture_semantic), 0, &ai_path_str);
            if (result != AI_SUCCESS) { continue; } //- this type of texture is not present in the material
            std::filesystem::path texture_path = m_asset_directory_path / ai_path_str.C_Str();
            if (texture_resource_map.contains(texture_path.string())) { continue; }
            if (not std::filesystem::exists(texture_path)) {
                return std::make_error_code(std::errc::no_such_file_or_directory);
            }
            auto image_sp = Image::makeShared();
            const aiTexture * ai_texture_p = m_ai_scene_p->GetEmbeddedTexture(ai_path_str.C_Str());
            if (ai_texture_p) {
                auto [data_bytes, format, width] = interpret_ai_texture(*ai_texture_p);
                image_sp->loadFromMemory(data_bytes, format, width);
            } else {
                image_sp->loadFromFile(texture_path);
            }
            //todo? check format to see if conversion is needed
            texture_resource_map[texture_path.string()] = image_sp;
            material.setTextureResource(texture_semantic, image_sp);
        }
    }
    return std::nullopt;
}

void ModelLoader::Impl::processGeometries(Model &model) noexcept
{
    for (size_t i = 0; i < m_ai_scene_p->mNumMeshes; ++i) {
        process_mesh(*m_geometry_resource_list[i], *m_ai_scene_p->mMeshes[i]);
    }
}

void ModelLoader::Impl::processMaterials(Model &model) noexcept
{
    for (size_t i = 0; i < m_ai_scene_p->mNumMaterials; ++i) {
        process_material(*m_material_resource_list[i], *m_ai_scene_p->mMaterials[i]);
    }
}

void ModelLoader::Impl::buildModel(Model &model) noexcept
{
    std::queue<AssimpHierarchyNode> hierarchy_node_queue;
    hierarchy_node_queue.emplace(-1, m_ai_scene_p->mRootNode);
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
   
    uint32_t geometry_num = m_ai_scene_p->mNumMeshes;
    uint32_t material_num = m_ai_scene_p->mNumMaterials; //- at least one material is guaranteed if AI_SCENE_FLAGS_INCOMPLETE flag is not set
    m_geometry_resource_list.resize(geometry_num);
    m_material_resource_list.resize(material_num);
    stdr::for_each(m_geometry_resource_list, [](auto & geometry_sp) { geometry_sp = Geometry::makeShared(); });
    stdr::for_each(m_material_resource_list, [](auto & material_sp) { material_sp = Material::makeShared(); });

    auto & render_primitive_list = model.m_render_primitive_list;
    render_primitive_list.resize(geometry_num);
    for (auto && [i, ai_mesh_p] : std::span(m_ai_scene_p->mMeshes, m_ai_scene_p->mNumMeshes) | stdv::enumerate) {
        auto & render_primitive = render_primitive_list[i];
        render_primitive.setGeometryResource(m_geometry_resource_list[i]);
        render_primitive.setMaterialResource(m_material_resource_list[ai_mesh_p->mMaterialIndex]);
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

constexpr uint32_t hash4(const char* str) noexcept
{
    return (str[0] << 24) | (str[1] << 16) | (str[2] << 8) | str[3];
}

std::tuple<std::span<const std::byte>, ImageFormat, uint32_t> interpret_ai_texture(const aiTexture & ai_texture)
{
    uint32_t width = ai_texture.mWidth;
    uint32_t height = ai_texture.mHeight;
    ImageFormat format = ImageFormat::eRGBA8Uint;
    bool is_compressed = (height == 0);
    switch (hash4(ai_texture.achFormatHint)) {
        case hash4("argb"): { format = ImageFormat::eARGB8Uint; } break;
        case hash4("bgra"): { format = ImageFormat::eBGRA8Uint; } break;
        case hash4("rgb"): { format = ImageFormat::eRGB8Uint; } break;
        case hash4("bgr"): { format = ImageFormat::eBGR8Uint; } break;
        default: break;
    }
    size_t size_in_bytes = is_compressed ? width : width * height * sizeof(aiTexel);
    auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(ai_texture.pcData), size_in_bytes);
    if (is_compressed) {
        width /= enum_decode::get_bytes_per_channel(format);
    }
    return std::make_tuple(bytes, format, width);
}

Model::HierarchyNode to_hierarchy_node(const AssimpHierarchyNode & ai_hierarchy_node)
{
    Model::HierarchyNode hierarchy_node;
    auto [parent_index, ai_node] = ai_hierarchy_node;
    hierarchy_node.m_parent_index = parent_index;
    hierarchy_node.m_primitive_indices.assign_range(std::span(ai_node->mMeshes, ai_node->mNumMeshes));
    hierarchy_node.m_local_matrix = to_matrix4x4(ai_node->mTransformation);
    return hierarchy_node;
}

void process_mesh(Geometry & geometry, const aiMesh & ai_mesh)
{
    uint32_t vertex_num = ai_mesh.mNumVertices;
    geometry.resize(vertex_num);
    std::vector<uint32_t> indices;
    for (const auto & face : std::span(ai_mesh.mFaces, ai_mesh.mNumFaces)) {
        geometry.addFace({static_cast<uint32_t>(indices.size()), static_cast<uint32_t>(face.mNumIndices)});
        indices.append_range(std::span(face.mIndices, face.mNumIndices));
    }
    geometry.setIndices(std::move(indices))
        .setAttributes<VertexAttribute::ePosition>(std::span(ai_mesh.mVertices, vertex_num))
        .setAttributes<VertexAttribute::eNormal>(std::span(ai_mesh.mNormals, vertex_num));
    // for (auto i : {0, 1}) {
    for (auto i : {0}) { //! temp
        if (not ai_mesh.HasTextureCoords(i)) { break; }
        auto span = std::span(ai_mesh.mTextureCoords[0], vertex_num);
        for (auto && [i, ai_texture_coord] : stdv::enumerate(span)) {
            geometry.setAttribute<VertexAttribute::eTexCoord0>(i, {ai_texture_coord.x, ai_texture_coord.y});
        }
    }
}

void process_material(Material & material, const aiMaterial & ai_material)
{
    for (auto property_p : std::span(ai_material.mProperties, ai_material.mNumProperties)) {
        lcf_log_info("key: {}, semantic: {}, index: {}, data_length: {}, type: {}",
            property_p->mKey.C_Str(),
            property_p->mSemantic,
            property_p->mIndex,
            property_p->mDataLength,
            enum_name(property_p->mType)
        );
        
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

