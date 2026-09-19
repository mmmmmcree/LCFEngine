#include "log.h"
#include "app/ShaderToyApp.h"
#include "shader_core/config.h"
#include <filesystem>

int main()
{
    lcf::log::init();

    std::filesystem::path shader_assets_dir = SHADER_ASSETS_DIR;
    std::filesystem::path image_assets_dir = IMAGE_ASSETS_DIR;
    lcf::sc::Config::instance()
        .registerVirtualPath("shaders", shader_assets_dir)
        .registerVirtualPath("images", image_assets_dir)
        .setDefaultGlslEntryPoint("main");

    lcf::shader_toy::App app;
    if (auto ec = app.create()) {
        lcf_log_error("Failed to create shader toy app: {}", ec.message());
        return 1;
    }
    if (auto ec = app.run()) {
        lcf_log_error("Failed to run shader toy app: {}", ec.message());
    }
    return 0;
}
