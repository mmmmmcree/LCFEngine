#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"
#include "common/render_enums.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanSamplerParams
    {
        using Self = VulkanSamplerParams;
    public:
        struct Hasher
        {
            uint64_t operator()(const Self & params) const noexcept;
        };
        static VulkanSamplerParams from_preset(SamplerPreset preset);
        VulkanSamplerParams(
            vk::Filter min_filter = vk::Filter::eNearest,
            vk::Filter mag_filter = vk::Filter::eNearest,
            vk::SamplerMipmapMode mipmap_mode = vk::SamplerMipmapMode::eNearest,
            vk::SamplerAddressMode address_mode_u = vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode address_mode_v = vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode address_mode_w = vk::SamplerAddressMode::eRepeat,
            float mip_lod_bias = {},
            bool anisotropy_enable = {},
            float max_anisotropy = {},
            bool compare_enable = {},
            vk::CompareOp compare_op = vk::CompareOp::eNever,
            float min_lod = {},
            float max_lod = {},
            vk::BorderColor border_color = vk::BorderColor::eFloatTransparentBlack,
            bool unnormalized_coordinates = {}
        );
        VulkanSamplerParams(const vk::SamplerCreateInfo & sampler_info);
        ~VulkanSamplerParams() = default;
        VulkanSamplerParams(const Self & other) = default;
        Self & operator=(const Self & other) = default;
        VulkanSamplerParams(Self && other) = default;
        Self & operator=(Self && other) = default;
        bool operator==(const Self & other) const noexcept;
        bool operator!=(const Self & other) const noexcept;
        vk::SamplerCreateInfo toCreateInfo() const noexcept;
        Self & setMinFilter(vk::Filter filter) noexcept { m_min_filter = filter; return *this; }
        Self & setMagFilter(vk::Filter filter) noexcept { m_mag_filter = filter; return *this; }
        Self & setMipmapMode(vk::SamplerMipmapMode mode) noexcept { m_mipmap_mode = mode; return *this; }
        Self & setAddressModeU(vk::SamplerAddressMode mode) noexcept { m_address_mode_u = mode; return *this; }
        Self & setAddressModeV(vk::SamplerAddressMode mode) noexcept { m_address_mode_v = mode; return *this; }
        Self & setAddressModeW(vk::SamplerAddressMode mode) noexcept { m_address_mode_w = mode; return *this; }
        Self & setMipLodBias(float bias) noexcept { m_mip_lod_bias = bias; return *this; }
        Self & setAnisotropyEnable(bool enable) noexcept { m_anisotropy_enable = enable; return *this; }
        Self & setMaxAnisotropy(float max_anisotropy) noexcept { m_max_anisotropy = max_anisotropy; return *this; }
        Self & setCompareEnable(bool enable) noexcept { m_compare_enable = enable; return *this; }
        Self & setCompareOp(vk::CompareOp op) noexcept { m_compare_op = op; return *this; }
        Self & setMinLod(float lod) noexcept { m_min_lod = lod; return *this; }
        Self & setMaxLod(float lod) noexcept { m_max_lod = lod; return *this; }
        Self & setBorderColor(vk::BorderColor color) noexcept { m_border_color = color; return *this; }
        Self & setUnnormalizedCoordinates(bool enable) noexcept { m_unnormalized_coordinates = enable; return *this; }
        vk::Filter getMinFilter() const noexcept { return m_min_filter; }
        vk::Filter getMagFilter() const noexcept { return m_mag_filter; }
        vk::SamplerMipmapMode getMipmapMode() const noexcept { return m_mipmap_mode; }
        vk::SamplerAddressMode getAddressModeU() const noexcept { return m_address_mode_u; }
        vk::SamplerAddressMode getAddressModeV() const noexcept { return m_address_mode_v; }
        vk::SamplerAddressMode getAddressModeW() const noexcept { return m_address_mode_w; }
        float getMipLodBias() const noexcept { return m_mip_lod_bias; }
        bool getAnisotropyEnable() const noexcept { return m_anisotropy_enable; }
        float getMaxAnisotropy() const noexcept { return m_max_anisotropy; }
        bool getCompareEnable() const noexcept { return m_compare_enable; }
        vk::CompareOp getCompareOp() const noexcept { return m_compare_op; }
        float getMinLod() const noexcept { return m_min_lod; }
        float getMaxLod() const noexcept { return m_max_lod; }
        vk::BorderColor getBorderColor() const noexcept { return m_border_color; }
        bool isUnnormalizedCoordinates() const noexcept { return m_unnormalized_coordinates; }
    private:
        vk::Filter m_min_filter;
        vk::Filter m_mag_filter;
        vk::SamplerMipmapMode m_mipmap_mode;
        vk::SamplerAddressMode m_address_mode_u;
        vk::SamplerAddressMode m_address_mode_v;
        vk::SamplerAddressMode m_address_mode_w;
        float m_mip_lod_bias;
        bool m_anisotropy_enable;
        float m_max_anisotropy;
        bool m_compare_enable;
        vk::CompareOp m_compare_op;
        float m_min_lod;
        float m_max_lod;
        vk::BorderColor m_border_color;
        bool m_unnormalized_coordinates;
    };

    class VulkanSampler : public STDPointerDefs<VulkanSampler>
    {
        using Self = VulkanSampler;
    public:
        VulkanSampler() = default;
        ~VulkanSampler() = default;
        VulkanSampler(const Self & other) = delete;
        Self & operator=(const Self & other) = delete;
        VulkanSampler(Self && other) = default;
        Self & operator=(Self && other) = default;
        bool create(VulkanContext * context_p, const vk::SamplerCreateInfo & create_info);
        bool isCreated() const noexcept { return static_cast<bool>(m_sampler); }
        const vk::Sampler & getHandle() const noexcept { return m_sampler.get(); }
        const VulkanSamplerParams & getParams() const noexcept { return m_params; }
    private:
        vk::UniqueSampler m_sampler;
        VulkanSamplerParams m_params;
    };
}