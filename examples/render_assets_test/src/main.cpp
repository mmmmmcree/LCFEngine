#include "log.h"
#include "render_assets/ModelLoader.h"
#include "Registry.h"

#include "glsl_type_traits.h"
#include "render_assets/Material.h"

using namespace lcf;
namespace stdr = std::ranges;

int main() {
    lcf::Logger::init();
    
    Material material;
    auto segments = material.generateInterleavedSegments<glsl::std430::enum_value_type_mapping_t>(ShadingModel::eStandard);
    std::vector<float> material_params(segments.getUpperBoundInBytes() / sizeof(float));
    for (const auto & segment : segments) {
        std::ranges::copy(segment.getDataView(), as_bytes(material_params).begin() + segment.getBeginOffsetInBytes());
    }

    for (auto v : material_params) {
        std::cout << v << ' ';
    }

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
