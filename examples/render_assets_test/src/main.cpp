#include "log.h"
#include "render_assets/ModelLoader.h"
#include "Registry.h"

#include "glsl_type_traits.h"
#include "render_assets/Material.h"

using namespace lcf;
namespace stdr = std::ranges;

#include "image/Image.h"

int main() {
    lcf::Logger::init();
    
    Registry registry;

    ModelLoader loader;
    auto model_opt = loader.load("./assets/models/BarbieDodgePickup/scene.gltf");
    if (not model_opt) {
        lcf_log_error("Failed to load model");
        return 1;
    }
    auto & model = model_opt.value();
    auto entities = model.generateEntities(registry);
    auto entity = model.generateEntity(registry);
    
    return 0;
}
