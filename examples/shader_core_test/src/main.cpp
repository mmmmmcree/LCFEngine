#include "shader_core/config.h"
#include "shader_core/ShaderCompiler.h"
#include "log.h"
#include <filesystem>

#include <chrono>

using namespace std::chrono; 
using namespace lcf;

int main()
{
    lcf::log::init();

    std::filesystem::path assets_dir = LCF_EXAMPLES_ASSETS_DIR;
    shader_core::Config::instance()
        .registerVirtualPath("shaders", assets_dir / "shaders");

    ShaderCompiler compiler;
    auto current_time = system_clock::now();
    auto compiled_spv = compiler.compileSlangSourceToSpv("shaders://sphere_to_cube.slang");
    auto elapsed_time = duration_cast<milliseconds>(system_clock::now() - current_time).count();
    lcf_log_info("Time elapsed: {} ms", elapsed_time);
    if (not compiled_spv) {
        lcf_log_info("Failed to compile shader: {}", compiled_spv.error().message());
    } else {
        lcf_log_info("Successfully compiled shader");
    }
    return 0;
}