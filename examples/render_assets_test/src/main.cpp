#include "log.h"
#include "render_assets/ModelLoader.h"
#include "Registry.h"

#include "glsl_type_traits.h"
#include "render_assets/Material.h"

using namespace lcf;
namespace stdr = std::ranges;

#include "image/Image.h"
#include "Vector.h"

int main() {
    lcf::Logger::init();
    
    Registry registry;

    ModelLoader loader;
    auto model_opt = loader.load("./assets/models/BarbieDodgePickup/scene.gltf");
    if (model_opt) {
        auto & model = model_opt.value();
        model.generateEntities(registry);
    } else {
        lcf_log_error("Failed to load model");
    }
    return 0;
}
