#pragma once

#include "RenderableModel.h"
#include <filesystem>
#include <optional>
#include <expected>
#include <system_error>

namespace lcf {
    class RenderableModelLoader
    {
        using Self = RenderableModelLoader;
    public:
        RenderableModelLoader();
        ~RenderableModelLoader();
        RenderableModelLoader(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        RenderableModelLoader(Self &&) = default;
        Self & operator=(Self &&) = default;
        std::optional<RenderableModel> load(const std::filesystem::path & path) const noexcept;
        std::expected<RenderableModel, std::error_code> analyze(const std::filesystem::path & path) const noexcept;
        void processGeometries(RenderableModel & model) const noexcept;
        void processMaterials(RenderableModel & model) const noexcept;
    private:
        void buildModel(RenderableModel & model) const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl_up;
    };
}