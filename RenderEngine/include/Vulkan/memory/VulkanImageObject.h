#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_fwd_decls.h"
#include "resource_utils.h"
#include <memory>

namespace lcf::render {
    class VulkanImageProxy;

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
        Self & addImageFlags(vk::ImageCreateFlags flags) noexcept;
        Self & setFormat(vk::Format format) noexcept;
        Self & setExtent(vk::Extent3D extent) noexcept;
        Self & setMipmapped(bool mipmapped) noexcept;
        Self & setArrayLayers(uint16_t array_layers) noexcept;
        Self & setSamples(vk::SampleCountFlagBits samples) noexcept;
        Self & setUsage(vk::ImageUsageFlags usage) noexcept;
        bool isCreated() const noexcept;
        ResourceLease lease() const noexcept;
        vk::Image getHandle() const noexcept;
        std::span<std::byte> getMappedMemorySpan() const noexcept;
        vk::ImageView getDefaultView() const;
        vk::ImageView getView(const vk::ImageSubresourceRange & subresource_range) const;
        vk::ImageCreateFlags getImageFlags() const;
        vk::ImageType getImageType() const noexcept;
        vk::Format getFormat() const;
        vk::Extent3D getExtent() const;
        uint32_t getMipLevelCount() const noexcept;
        uint32_t getArrayLayerCount() const;
        vk::SampleCountFlagBits getSamples() const;
        vk::ImageAspectFlags getAspectFlags() const noexcept;
        std::optional<vk::ImageLayout> getLayout() const noexcept;
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout);
        void transitLayout(VulkanCommandBufferObject & cmd, const vk::ImageSubresourceRange & subresource_range, vk::ImageLayout new_layout);
        void copyFrom(VulkanCommandBufferObject & cmd, vk::Buffer buffer, std::span<const vk::BufferImageCopy> regions);
    private:
        std::shared_ptr<VulkanImageProxy> m_proxy_sp;
    };
}
