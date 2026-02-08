#include "ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <magic_enum/magic_enum.hpp>

using namespace lcf;

Model ModelLoader::load(std::string_view path)
{
    m_model_dir = std::filesystem::path(path).parent_path();
    Assimp::Importer importer;
    const aiScene* ai_scene = importer.ReadFile(path.data(),
        aiProcess_Triangulate |           // 三角化所有面
        aiProcess_GenNormals |            // 生成法线(如果没有)
        aiProcess_FlipUVs |               // 翻转UV坐标
        aiProcess_CalcTangentSpace |      // 计算切线和副切线
        aiProcess_JoinIdenticalVertices | // 合并重复顶点
        aiProcess_OptimizeMeshes          // 优化网格
    );
    if (not ai_scene or ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE or not ai_scene->mRootNode) {
        //! handle error
        return {};
    }
    // for (uint32_t i = 0; i < ai_scene->mNumMaterials; ++i) {
    //     model.addMaterial(this->processMaterial(*ai_scene->mMaterials[i], *ai_scene));
    // }
    Model model;
    for (uint32_t i = 0; i < ai_scene->mNumMeshes; ++i) {
        model.addMesh(this->processMesh(*ai_scene->mMeshes[i]));
    }
    return model;
}

Material::UniquePointer ModelLoader::processMaterial(const aiMaterial & ai_material, const aiScene &ai_scene)
{
    Material::UniquePointer material_up = Material::makeUnique();
    return material_up;
}

Mesh::UniquePointer lcf::ModelLoader::processMesh(const aiMesh &ai_mesh)
{
    Mesh::UniquePointer mesh_up = Mesh::makeUnique();
    uint32_t vertex_count = ai_mesh.mNumVertices;
    mesh_up->create(vertex_count);
    mesh_up->setVertexData(VertexSemanticFlags::ePosition, as_bytes_from_ptr(ai_mesh.mVertices, vertex_count));
    mesh_up->setVertexData(VertexSemanticFlags::eNormal, as_bytes_from_ptr(ai_mesh.mNormals, vertex_count));
    if (ai_mesh.HasTextureCoords(0)) {
        mesh_up->setVertexData(VertexSemanticFlags::eTexCoord0, as_bytes_from_ptr(ai_mesh.mTextureCoords[0], vertex_count));
    }
    std::vector<uint32_t> indices;
    for (unsigned int i = 0; i < ai_mesh.mNumFaces; ++i) {
        aiFace face = ai_mesh.mFaces[i];
        mesh_up->addFace({static_cast<uint16_t>(indices.size()), static_cast<uint16_t>(face.mNumIndices)});
        indices.insert(indices.end(), face.mIndices, face.mIndices + face.mNumIndices);
    }
    mesh_up->setIndexData(indices);
    return mesh_up;
}
