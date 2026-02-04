#pragma once

#include "Model.h"
#include <filesystem>
#include <optional>
#include <expected>
#include <system_error>

namespace lcf {
    class ModelLoader
    {
        using Self = ModelLoader;
    public:
        ModelLoader();
        ~ModelLoader();
        ModelLoader(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        ModelLoader(Self &&) = default;
        Self & operator=(Self &&) = default;
        std::optional<Model> load(const std::filesystem::path & path) const noexcept;
        // std::expected<Model, std::error_code> analyze(const std::filesystem::path & path) const noexcept;
        // void processGeometries(Model & model) const noexcept;
        // void processMaterials(Model & model) const noexcept;
    // private:
    //     void buildModel(Model & model) const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl_up;
    };
}