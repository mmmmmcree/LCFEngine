#include "log.h"
#include "app/ShaderToyApp.h"
#include "shader_core/config.h"
#include <filesystem>

int main()
{
    lcf::log::init();

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
