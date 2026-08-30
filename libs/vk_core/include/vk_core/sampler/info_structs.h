#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/utils/DynamicStructureChain.h"

namespace lcf::vkc {

class SamplerInfo
{
    using Self = SamplerInfo;
    using Root = vk::SamplerCreateInfo;
public:
    ~SamplerInfo() noexcept = default;
    SamplerInfo(
        vk::SamplerCreateFlags flags = {},
        vk::Filter mag_filter = vk::Filter::eNearest,
        vk::Filter min_filter = vk::Filter::eNearest,
        vk::SamplerMipmapMode mipmap_mode = vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode address_mode_u = vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode address_mode_v = vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode address_mode_w = vk::SamplerAddressMode::eRepeat,
        float mip_lod_bias = 0.0f,
        vk::Bool32 anisotropy_enable = false,
        float max_anisotropy = 1.0f,
        vk::Bool32 compare_enable = false,
        vk::CompareOp compare_op = vk::CompareOp::eNever,
        float min_lod = 0.0f,
        float max_lod = 0.0f,
        vk::BorderColor border_color = vk::BorderColor::eFloatTransparentBlack,
        vk::Bool32 unnormalized_coordinates = false) noexcept
    {
        m_sampler.root()
            .setFlags(flags)
            .setMagFilter(mag_filter)
            .setMinFilter(min_filter)
            .setMipmapMode(mipmap_mode)
            .setAddressModeU(address_mode_u)
            .setAddressModeV(address_mode_v)
            .setAddressModeW(address_mode_w)
            .setMipLodBias(mip_lod_bias)
            .setAnisotropyEnable(anisotropy_enable)
            .setMaxAnisotropy(max_anisotropy)
            .setCompareEnable(compare_enable)
            .setCompareOp(compare_op)
            .setMinLod(min_lod)
            .setMaxLod(max_lod)
            .setBorderColor(border_color)
            .setUnnormalizedCoordinates(unnormalized_coordinates);
    }
    SamplerInfo(const Self &) = default;
    SamplerInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
    operator const Root &() const noexcept { return m_sampler.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_sampler.template request<T>(); }
    Self & addFlags(vk::SamplerCreateFlags flags) noexcept { m_sampler.root().setFlags(m_sampler.root().flags | flags); return *this; }
    Self & setMagFilter(vk::Filter filter) noexcept { m_sampler.root().setMagFilter(filter); return *this; }
    Self & setMinFilter(vk::Filter filter) noexcept { m_sampler.root().setMinFilter(filter); return *this; }
    Self & setMinMagFilter(vk::Filter min_filter, vk::Filter mag_filter) noexcept { return this->setMinFilter(min_filter).setMagFilter(mag_filter); }
    Self & setMipmapMode(vk::SamplerMipmapMode mode) noexcept { m_sampler.root().setMipmapMode(mode); return *this; }
    Self & setAddressModeU(vk::SamplerAddressMode mode) noexcept { m_sampler.root().setAddressModeU(mode); return *this; }
    Self & setAddressModeV(vk::SamplerAddressMode mode) noexcept { m_sampler.root().setAddressModeV(mode); return *this; }
    Self & setAddressModeW(vk::SamplerAddressMode mode) noexcept { m_sampler.root().setAddressModeW(mode); return *this; }
    Self & setAddressMode(
        vk::SamplerAddressMode address_mode_u,
        vk::SamplerAddressMode address_mode_v,
        vk::SamplerAddressMode address_mode_w) noexcept
    {
        return this->setAddressModeU(address_mode_u)
            .setAddressModeV(address_mode_v)
            .setAddressModeW(address_mode_w);
    }
    Self & setAddressMode(vk::SamplerAddressMode mode) noexcept { return this->setAddressMode(mode, mode, mode); }
    Self & setMipLodBias(float bias) noexcept { m_sampler.root().setMipLodBias(bias); return *this; }
    Self & setAnisotropy(vk::Bool32 enable, float max_anisotropy = 1.0f) noexcept
    {
        m_sampler.root().setAnisotropyEnable(enable).setMaxAnisotropy(max_anisotropy);
        return *this;
    }
    Self & setCompare(vk::Bool32 enable, vk::CompareOp op = vk::CompareOp::eNever) noexcept
    {
        m_sampler.root().setCompareEnable(enable).setCompareOp(op);
        return *this;
    }
    Self & setLodRange(float min_lod, float max_lod) noexcept
    {
        m_sampler.root().setMinLod(min_lod).setMaxLod(max_lod);
        return *this;
    }
    Self & setBorderColor(vk::BorderColor color) noexcept { m_sampler.root().setBorderColor(color); return *this; }
    Self & setUnnormalizedCoordinates(vk::Bool32 enable) noexcept { m_sampler.root().setUnnormalizedCoordinates(enable); return *this; }
    const vk::SamplerCreateFlags & getFlags() const noexcept { return m_sampler.root().flags; }
    const vk::Filter & getMagFilter() const noexcept { return m_sampler.root().magFilter; }
    const vk::Filter & getMinFilter() const noexcept { return m_sampler.root().minFilter; }
    const vk::SamplerMipmapMode & getMipmapMode() const noexcept { return m_sampler.root().mipmapMode; }
    const vk::SamplerAddressMode & getAddressModeU() const noexcept { return m_sampler.root().addressModeU; }
    const vk::SamplerAddressMode & getAddressModeV() const noexcept { return m_sampler.root().addressModeV; }
    const vk::SamplerAddressMode & getAddressModeW() const noexcept { return m_sampler.root().addressModeW; }
    const float & getMipLodBias() const noexcept { return m_sampler.root().mipLodBias; }
    const vk::Bool32 & isAnisotropyEnabled() const noexcept { return m_sampler.root().anisotropyEnable; }
    const float & getMaxAnisotropy() const noexcept { return m_sampler.root().maxAnisotropy; }
    const vk::Bool32 & isCompareEnabled() const noexcept { return m_sampler.root().compareEnable; }
    const vk::CompareOp & getCompareOp() const noexcept { return m_sampler.root().compareOp; }
    const float & getMinLod() const noexcept { return m_sampler.root().minLod; }
    const float & getMaxLod() const noexcept { return m_sampler.root().maxLod; }
    const vk::BorderColor & getBorderColor() const noexcept { return m_sampler.root().borderColor; }
    const vk::Bool32 & isUnnormalizedCoordinatesEnabled() const noexcept { return m_sampler.root().unnormalizedCoordinates; }
private:
    utils::DynamicStructureChain<Root> m_sampler;
};

} // namespace lcf::vkc
