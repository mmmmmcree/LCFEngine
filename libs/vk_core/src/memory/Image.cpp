#include "vk_core/memory/Image.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/error.h"


namespace lcf::vkc {

std::error_code ImageView::create(
    vk::Device device,
    ImageResourceHandle image_resource_handle,
    vk::ImageViewType view_type,
    vk::Format format,
    const vk::ImageSubresourceRange & range) noexcept
{
    m_image_rh = std::move(image_resource_handle);
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(m_image_rh->handle())
        .setViewType(view_type)
        .setFormat(format)
        .setSubresourceRange(range);
    vk::ImageView view;
    try {
        view = device.createImageView(view_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    m_view_rh = ResourceHandle {view, [device, view, image_lease = m_image_rh.lease()]() mutable noexcept {
        device.destroyImageView(view);
        image_lease = {};
    }};
    m_view_type = view_type;
    m_format = format;
    m_range = range;
    return {};
}

std::error_code Image::create(
    const MemoryAllocator & allocator,
    const vk::ImageCreateInfo & image_info,
    const MemoryAllocationInfo & alloc_info) noexcept
{
    if (m_memory_rh) { return make_error_code(errc::already_created); }
    auto expected_memory = allocator.allocateImage(image_info, alloc_info);
    if (not expected_memory) { return expected_memory.error(); }
    m_memory_rh = std::move(expected_memory.value());
    m_device = allocator.getDevice();
    m_desc = image_info;
    return {};
}

Image::operator vk::Image() const noexcept
{
    return m_memory_rh->handle();
}

ResourceLease Image::lease() const noexcept
{
    return m_memory_rh.lease();
}

const vk::Image & Image::handle() const noexcept
{
    return m_memory_rh->handle();
}

std::expected<ImageView, std::error_code> Image::createView(
    const vk::ImageSubresourceRange & range, vk::ImageViewType view_type) const noexcept
{
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(m_memory_rh->handle())
        .setViewType(view_type)
        .setFormat(m_desc.getFormat())
        .setSubresourceRange(range);
    ImageView view;
    if (auto ec = view.create(m_device, m_memory_rh, view_type, m_desc.getFormat(), range)) {
        return std::unexpected(ec);
    }
    return view;
}

std::expected<vk::UniqueImageView, std::error_code> Image::createUniqueView(
    const vk::ImageSubresourceRange & range, vk::ImageViewType view_type) const noexcept
{
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(m_memory_rh->handle())
        .setViewType(view_type)
        .setFormat(m_desc.getFormat())
        .setSubresourceRange(range);
    try {
        return m_device.createImageViewUnique(view_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    return {};
}

std::error_code Attachment::create(const Image & image, const AttachmentDescription & desc) noexcept
{
    auto expected_view = image.createUniqueView(desc.getSubresourceRange(), desc.getViewType());
    if (not expected_view) { return expected_view.error(); }
    m_image = image;
    m_view = std::move(expected_view.value());
    m_desc = desc;
    return {};
}

} // namespace lcf::vkc
