// #include "log.h"
// #include "render_assets/ModelLoader.h"
// #include "Registry.h"

// #include "glsl_type_traits.h"
// #include "render_assets/Material.h"

// using namespace lcf;
// namespace stdr = std::ranges;

// int main() {
//     lcf::Logger::init();
    
//     Material material;
//     auto segments = material.generateInterleavedSegments<glsl::std430::enum_value_type_mapping_t>(ShadingModel::eStandard);
//     std::vector<float> material_params(segments.getUpperBoundInBytes() / sizeof(float));
//     for (const auto & segment : segments) {
//         std::ranges::copy(segment.getDataView(), as_bytes(material_params).begin() + segment.getBeginOffsetInBytes());
//     }

//     for (auto v : material_params) {
//         std::cout << v << ' ';
//     }

//     Registry registry;

//     ModelLoader loader;
//     auto model_opt = loader.load("./assets/models/BarbieDodgePickup/scene.gltf");
//     if (model_opt) {
//         auto & model = model_opt.value();
//         model.generateEntities(registry);
//     } else {
//         lcf_log_error("Failed to load model");
//     }
//     return 0;
// }


#include "image2/Image.h"
#include "enums/enum_name.h"
#include "log.h"
#include <boost/gil/extension/toolbox/color_spaces/gray_alpha.hpp>
#include <boost/gil.hpp>

using namespace lcf;
int main()
{
    lcf::Logger::init();
    boost::gil::gray_alpha16_image_t img;

    // auto info1 = gil::read_image_info("./assets/models/BarbieDodgePickup/textures/ALs_CARS_baseColor.png", gil::png_tag());

    // auto info2 = gil::read_image_info("./assets/models/BarbieDodgePickup/textures/texture_1_INT_EMB_baseColor.jpeg", gil::jpeg_tag());
    
    // std::cout << info1._info._color_type << ' ' << info1._info._width << ' ' << info1._info._height << ' ' << static_cast<uint32_t>(info1._info._num_channels) << std::endl;

    // std::cout << info2._info._color_space << ' ' << info2._info._width << ' ' << info2._info._height << ' ' << info2._info._num_components << std::endl;
    ImageInfo png_info {"./assets/models/BarbieDodgePickup/textures/ALs_CARS_baseColor.png"};
    lcf_log_info("width: {}, height: {}, num_channels: {}", png_info.getWidth(), png_info.getHeight(), png_info.getChannelCount());
    

    ImageInfo jpeg_info {"./assets/models/BarbieDodgePickup/textures/texture_1_INT_EMB_baseColor.jpeg"};
    lcf_log_info("width: {}, height: {}, num_channels: {}", jpeg_info.getWidth(), jpeg_info.getHeight(), jpeg_info.getChannelCount());
    
    return 0;
}