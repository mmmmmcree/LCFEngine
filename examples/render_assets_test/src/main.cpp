#include "log.h"
#include "render_assets/ModelLoader.h"

#include "glsl_type_traits.h"
#include "render_assets/Material.h"
#include "render_assets/Texture2D.h"

using namespace lcf;
namespace stdr = std::ranges;

#include "image/Image.h"

int main() {
    lcf::Logger::init();
    

    ModelLoader loader;
    auto model_opt = loader.load("./assets/models/BarbieDodgePickup/scene.gltf");
    if (not model_opt) {
        lcf_log_error("Failed to load model");
        return 1;
    }
    const auto & model = *model_opt;
    model.m_render_primitive_list[13]
        .getMaterial()
        .getTextureResource(TextureSemantic::eEmissive)
        ->saveToFile("test.png");
    return 0;
}
