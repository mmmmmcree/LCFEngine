#include "vk_core/memory/Image.h"
#include "vk_core/memory/details/Memory.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"

namespace lcf::vkc {

std::error_code Image::create(
    const MemoryAllocator & allocator,
    const vk::ImageCreateInfo & image_info,
    const MemoryAllocationInfo & alloc_info) noexcept
{
    auto expected_memory = allocator.allocateImage(image_info, alloc_info);
    if (not expected_memory) { return expected_memory.error(); }
    m_memory_rp = std::move(expected_memory.value());
    m_device = allocator.getDevice();
    m_desc = image_info;
    return {};
}

Image::operator vk::Image() const noexcept
{
    return m_memory_rp->handle();
}

ResourceLease Image::lease() const noexcept
{
    return m_memory_rp.lease();
}

const vk::Image & Image::handle() const noexcept
{
    return m_memory_rp->handle();
}

std::expected<vk::UniqueImageView, std::error_code> Image::createView(
    const vk::ImageSubresourceRange & range, vk::ImageViewType view_type) const noexcept
{
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(m_memory_rp->handle())
        .setViewType(view_type)
        .setFormat(m_desc.getFormat())
        .setSubresourceRange(range);
    try {
        return m_device.createImageViewUnique(view_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
}

std::error_code Attachment::create(const Image & image, const AttachmentDescription & desc) noexcept
{
    auto expected_view = image.createView(desc.getSubresourceRange(), desc.getViewType());
    if (not expected_view) { return expected_view.error(); }
    m_image = image;
    m_view = std::move(expected_view.value());
    m_desc = desc;
    return {};
}

} // namespace lcf::vkc
