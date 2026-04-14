#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include "VulkanImageProxy.h"
#include <memory>

namespace lcf::render {
    class VulkanImageObject : public VulkanImageObjectPointerDefs
    {
        using Self = VulkanImageObject;
        friend class VulkanAttachment;
    public:
        VulkanImageObject();
        ~VulkanImageObject();
        VulkanImageObject(const VulkanImageObject & other) = default;
        VulkanImageObject & operator=(const VulkanImageObject & other) = default;
        VulkanImageObject(VulkanImageObject && other) noexcept = default;
        VulkanImageObject & operator=(VulkanImageObject && other) noexcept = default;
        bool create(VulkanContext * context_p);
        bool create(VulkanContext * context_p, vk::Image external_image);
        void setData(VulkanCommandBufferObject & cmd, std::span<const std::byte> data, uint32_t layer = 0);
        void generateMipmaps(VulkanCommandBufferObject & cmd);
        Self & addImageFlags(vk::ImageCreateFlags flags) { m_proxy_sp->m_flags |= flags; return *this; }
        Self & setFormat(vk::Format format) { m_proxy_sp->m_format = format; return *this; }
        Self & setExtent(vk::Extent3D extent) { m_proxy_sp->m_extent = extent; return *this; }
        Self & setMipmapped(bool mipmapped) { m_proxy_sp->m_mip_level_count = !mipmapped; return *this; }
        Self & setArrayLayers(uint16_t array_layers) { m_proxy_sp->m_array_layers = array_layers; return *this; }
        Self & setSamples(vk::SampleCountFlagBits samples) { m_proxy_sp->m_samples = samples; return *this; }
        Self & setUsage(vk::ImageUsageFlags usage) { m_proxy_sp->m_usage = usage; return *this; }
        bool isCreated() const noexcept { return m_proxy_sp and m_proxy_sp->isCreated(); }
        ResourceLease lease() const noexcept { return m_proxy_sp->lease(); }
        vk::Image getHandle() const noexcept { return m_proxy_sp->getHandle(); }
        std::span<std::byte> getMappedMemorySpan() const noexcept { return m_proxy_sp->getMappedMemorySpan(); }
        vk::ImageView getDefaultView() const { return m_proxy_sp->getDefaultView(); }
        vk::ImageView getView(const VulkanImageProxy::ImageViewKey & image_view_key) const { return m_proxy_sp->getView(image_view_key); }
        vk::ImageCreateFlags getImageFlags() const { return m_proxy_sp->getImageFlags(); }
        vk::ImageType getImageType() const noexcept { return m_proxy_sp->getImageType(); }
        vk::Format getFormat() const { return m_proxy_sp->getFormat(); }
        vk::Extent3D getExtent() const { return m_proxy_sp->getExtent(); }
        uint32_t getMipLevelCount() const noexcept { return m_proxy_sp->getMipLevelCount(); }
        uint32_t getArrayLayerCount() const { return m_proxy_sp->getArrayLayerCount(); }
        vk::SampleCountFlagBits getSamples() const { return m_proxy_sp->getSamples(); }
        vk::ImageAspectFlags getAspectFlags() const noexcept { return m_proxy_sp->getAspectFlags(); }
        std::optional<vk::ImageLayout> getLayout() const noexcept { return m_proxy_sp->getLayout(); }
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout) { m_proxy_sp->transitLayout(cmd, new_layout); }
        void transitLayout(VulkanCommandBufferObject & cmd, const vk::ImageSubresourceRange & subresource_range, vk::ImageLayout new_layout) { m_proxy_sp->transitLayout(cmd, subresource_range, new_layout); }
        void copyFrom(VulkanCommandBufferObject & cmd, vk::Buffer buffer, std::span<const vk::BufferImageCopy> regions) { m_proxy_sp->copyFrom(cmd, buffer, regions); }
    private:
        std::shared_ptr<VulkanImageProxy> m_proxy_sp;
    };
}
