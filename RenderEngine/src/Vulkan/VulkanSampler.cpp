#include "Vulkan/VulkanSampler.h"
#include "Vulkan/VulkanContext.h"
#include <boost/functional/hash.hpp>

using namespace lcf::render;

bool VulkanSampler::create(VulkanContext * context_p, const vk::SamplerCreateInfo &create_info)
{
    auto device = context_p->getDevice();
    vk::SamplerCreateInfo sampler_info = create_info;
    if (sampler_info.anisotropyEnable) {
        auto limits = context_p->getPhysicalDevice().getProperties().limits;
        sampler_info.maxAnisotropy = std::min(create_info.maxAnisotropy, limits.maxSamplerAnisotropy);
    }
    m_params = VulkanSamplerParams {sampler_info};
    m_sampler = device.createSamplerUnique(sampler_info);
    return this->isCreated();
}
    
lcf::render::VulkanSamplerParams::VulkanSamplerParams(
    vk::Filter min_filter,
    vk::Filter mag_filter,
    vk::SamplerMipmapMode mipmap_mode,
    vk::SamplerAddressMode address_mode_u,
    vk::SamplerAddressMode address_mode_v,
    vk::SamplerAddressMode address_mode_w,
    float mip_lod_bias,
    bool anisotropy_enable,
    float max_anisotropy,
    bool compare_enable,
    vk::CompareOp compare_op,
    float min_lod,
    float max_lod,
    vk::BorderColor border_color,
    bool unnormalized_coordinates) :
    m_min_filter(min_filter),
    m_mag_filter(mag_filter),
    m_mipmap_mode(mipmap_mode),
    m_address_mode_u(address_mode_u),
    m_address_mode_v(address_mode_v),
    m_address_mode_w(address_mode_w),
    m_mip_lod_bias(mip_lod_bias),
    m_anisotropy_enable(anisotropy_enable),
    m_max_anisotropy(max_anisotropy),
    m_compare_enable(compare_enable),
    m_compare_op(compare_op),
    m_min_lod(min_lod),
    m_max_lod(max_lod),
    m_border_color(border_color),
    m_unnormalized_coordinates(unnormalized_coordinates)
{
}

lcf::render::VulkanSamplerParams::VulkanSamplerParams(const vk::SamplerCreateInfo &sampler_info) :
    m_min_filter(sampler_info.minFilter),
    m_mag_filter(sampler_info.magFilter),
    m_mipmap_mode(sampler_info.mipmapMode),
    m_address_mode_u(sampler_info.addressModeU),
    m_address_mode_v(sampler_info.addressModeV),
    m_address_mode_w(sampler_info.addressModeW),
    m_mip_lod_bias(sampler_info.mipLodBias),
    m_anisotropy_enable(sampler_info.anisotropyEnable),
    m_max_anisotropy(sampler_info.maxAnisotropy),
    m_compare_enable(sampler_info.compareEnable),
    m_compare_op(sampler_info.compareOp),
    m_min_lod(sampler_info.minLod),
    m_max_lod(sampler_info.maxLod),
    m_border_color(sampler_info.borderColor),
    m_unnormalized_coordinates(sampler_info.unnormalizedCoordinates)
{
}

bool lcf::render::VulkanSamplerParams::operator==(const Self &other) const noexcept
{
    return Hasher{}(*this) == Hasher{}(other);
}

bool lcf::render::VulkanSamplerParams::operator!=(const Self &other) const noexcept
{
    return not (*this == other);
}

uint64_t lcf::render::VulkanSamplerParams::Hasher::operator()(const Self & params) const noexcept
{
    uint64_t seed = 0;
    const uint8_t* data = reinterpret_cast<const uint8_t *>(&params);
    for (size_t i = 0; i < sizeof(params); ++i) {
        boost::hash_combine(seed, data[i]);
    }
    return seed;
}

VulkanSamplerParams lcf::render::VulkanSamplerParams::from_preset(SamplerPreset preset)
{
    static const std::unordered_map<SamplerPreset, VulkanSamplerParams> s_presets {
        {
            SamplerPreset::eColorMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                0.0f,
                true,
                16.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eNormalMap,
            { 
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                0.0f,
                true,
                16.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eMaterialMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                0.0f,
                true,
                16.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eEmissiveMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                0.0f,
                false, 
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eEnvironmentMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eIrradianceMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eNearest,        // 辐照度图通常单层
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                0.0f,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::ePrefilteredMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eBRDFLUT,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eNearest,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                0.0f,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eUITexture,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eSpriteTexture,
            {
                vk::Filter::eNearest,                   // 像素艺术使用 nearest
                vk::Filter::eNearest,
                vk::SamplerMipmapMode::eNearest,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eShadowMap,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eNearest,
                vk::SamplerAddressMode::eClampToBorder,
                vk::SamplerAddressMode::eClampToBorder,
                vk::SamplerAddressMode::eClampToBorder,
                0.0f,
                false,
                1.0f,
                true,                                   // 启用深度比较
                vk::CompareOp::eLessOrEqual,            // PCF 阴影
                0.0f,
                0.0f,
                vk::BorderColor::eFloatOpaqueWhite,     // 边界外视为最大深度
                false
            }
        },
        {
            SamplerPreset::ePointSampled,
            {
                vk::Filter::eNearest,
                vk::Filter::eNearest,
                vk::SamplerMipmapMode::eNearest,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                0.0f,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eScreenTexture,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eNearest,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                0.0f,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        },
        {
            SamplerPreset::eBlurTexture,
            {
                vk::Filter::eLinear,
                vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                vk::SamplerAddressMode::eClampToEdge,
                0.0f,
                false,
                1.0f,
                false,
                vk::CompareOp::eNever,
                0.0f,
                VK_LOD_CLAMP_NONE,
                vk::BorderColor::eFloatTransparentBlack,
                false
            }
        }
    };
    return s_presets.at(preset);
}