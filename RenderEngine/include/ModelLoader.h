#pragma once

#include "Model.h"
#include "Image/Image.h"
#include <assimp/scene.h>
#include <unordered_map>
#include <filesystem>

namespace lcf {
    class ModelLoader
    {
    public:
        ModelLoader() = default;
        ModelLoader(const ModelLoader &) = delete;
        ModelLoader & operator=(const ModelLoader &) = delete;
        ModelLoader(ModelLoader &&) = default;
        ModelLoader & operator=(ModelLoader &&) = default;
        Model load(std::string_view path);
    private:
        Material::UniquePointer processMaterial(const aiMaterial & ai_material, const aiScene & ai_scene);
        Mesh::UniquePointer processMesh(const aiMesh & ai_mesh);
    private:
        std::filesystem::path m_model_dir;
        std::unordered_map<std::string, Image::SharedPointer> m_loaded_images;
    };
}