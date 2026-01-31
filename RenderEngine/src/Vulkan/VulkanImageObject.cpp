#include "Vulkan/VulkanImageObject.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanBufferObject.h"
#include "Vulkan/vulkan_utililtie.h"
#include "log.h"

using namespace lcf::render;

bool VulkanImageObject::create(VulkanContext * context_p)
{
   return this->_create(context_p, vk::ImageTiling::eOptimal, MemoryAllocationCreateInfo {vk::MemoryPropertyFlagBits::eDeviceLocal});
}

bool VulkanImageObject::create(VulkanContext *context_p, vk::Image external_image)
{
    m_context_p = context_p;
    m_image = external_image;
    auto interval = LayoutMapInterval::right_open(0, this->getMipLevelCount() * m_array_layers);
    m_layout_map.set(std::make_pair(interval, vk::ImageLayout::eUndefined));
    return this->isCreated();
}

void VulkanImageObject::setData(VulkanCommandBufferObject &cmd, std::span<const std::byte> data, uint32_t layer)
{
    VulkanBufferProxy staging_buffer;
    staging_buffer.setUsage(GPUBufferUsage::eStaging)
        .create(m_context_p, data.size_bytes());
    staging_buffer.writeSegmentDirectly(data);
    cmd.acquireResource(staging_buffer.getResource());
    vk::BufferImageCopy region;
    region.setImageSubresource({ this->getAspectFlags(), 0, 0, 1 })
        .setImageOffset({ 0, 0, 0 })
        .setImageExtent(m_extent);
    this->transitLayout(cmd, vk::ImageLayout::eTransferDstOptimal);
    cmd.copyBufferToImage(staging_buffer.getHandle(), this->getHandle(), vk::ImageLayout::eTransferDstOptimal, region);
    this->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
}

lcf::Image VulkanImageObject::readData()
{
    auto device = m_context_p->getDevice();
    vk::BufferImageCopy region;
    region.setImageSubresource({ this->getAspectFlags(), 0, 0, 1 })
        .setImageOffset({ 0, 0, 0 })
        .setImageExtent(m_extent);
    VulkanBufferProxy staging_buffer;
    auto [width, height, depth] = m_extent;
    staging_buffer.setUsage(GPUBufferUsage::eStaging)
        .create(m_context_p, width * height * 4);
    vkutils::immediate_submit(m_context_p, vk::QueueFlagBits::eGraphics, [&](VulkanCommandBufferObject & cmd) {
        auto old_layout = *this->getLayout();
        this->transitLayout(cmd, vk::ImageLayout::eTransferSrcOptimal);
        cmd.copyImageToBuffer(this->getHandle(), vk::ImageLayout::eTransferSrcOptimal, staging_buffer.getHandle(), region);
        this->transitLayout(cmd, old_layout);
    });
    Image image;
    image.loadFromMemory({staging_buffer.getMappedMemoryPtr(), staging_buffer.getSizeInBytes()}, ImageFormat::eRGBA8Uint, width);
    return image;
}

void VulkanImageObject::generateMipmaps(VulkanCommandBufferObject & cmd)
{
    using Offset3DPair = std::pair<vk::Offset3D, vk::Offset3D>;
    Offset3DPair src_offsets = {{0, 0, 0}, {static_cast<int32_t>(m_extent.width), static_cast<int32_t>(m_extent.height), 1}};
    auto aspect_flags = this->getAspectFlags();
    for (uint32_t level = 1; level < this->getMipLevelCount(); ++level) {
        Offset3DPair dst_offsets = {{0, 0, 0}, {static_cast<int32_t>(m_extent.width) >> level, static_cast<int32_t>(m_extent.height) >> level, 1}};
        this->blitTo(cmd, {aspect_flags, level - 1, 0, m_array_layers}, src_offsets,
            *this, {aspect_flags, level, 0, m_array_layers}, dst_offsets, vk::Filter::eLinear);
        src_offsets = dst_offsets;
    }
    this->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void VulkanImageObject::transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout)
{
    this->transitLayout(cmd, this->getFullResourceRange(), new_layout);
}

void VulkanImageObject::transitLayout(VulkanCommandBufferObject & cmd, const vk::ImageSubresourceRange &subresource_range, vk::ImageLayout new_layout)
{
    auto from_interval = [this](const LayoutMapIntervalType &interval) {
        uint32_t base_layer = interval.lower() / m_mip_level_count;
        uint32_t layer_count = (interval.upper() - 1) / m_mip_level_count - base_layer + 1;
        uint32_t base_level = interval.lower() % m_mip_level_count;
        uint32_t level_count = std::min<uint16_t>(interval.upper() - interval.lower(), m_mip_level_count);
        return std::make_tuple(base_layer, layer_count, base_level, level_count);
    };
    const auto &[aspect, base_level, level_count, base_layer, layer_count] = subresource_range;
    std::vector<vk::ImageMemoryBarrier2> barriers;
    auto intervals = this->getLayoutIntervals(base_layer, layer_count, base_level, level_count);
    for (const auto &target_interval : intervals) {
        auto [begin, end] = m_layout_map.equal_range(target_interval);
        for (const auto & [equal_interval, wrapped_layout] : std::ranges::subrange(begin, end)) {
            auto [layer, layer_count, level, level_count] = from_interval(equal_interval & target_interval);
            vk::ImageLayout old_layout = wrapped_layout.getValue();
            auto [src_stage, src_access, dst_stage, dst_access] = vkutils::get_image_layout_transition_dependency(old_layout, new_layout, cmd.getQueueType());
            vk::ImageMemoryBarrier2 barrier;
            barrier.setImage(this->getHandle())
                .setOldLayout(old_layout)
                .setNewLayout(new_layout)
                .setSubresourceRange({aspect, level, level_count, layer, layer_count})
                .setSrcStageMask(src_stage)
                .setSrcAccessMask(src_access)
                .setDstStageMask(dst_stage)
                .setDstAccessMask(dst_access);
            barriers.emplace_back(barrier);
        }
        m_layout_map.set(std::make_pair(target_interval, new_layout));
    }
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(barriers);
    cmd.pipelineBarrier2(dependency_info);
}

void VulkanImageObject::copyFrom(VulkanCommandBufferObject &cmd, vk::Buffer buffer, std::span<const vk::BufferImageCopy> regions)
{
    this->transitLayout(cmd, vk::ImageLayout::eTransferDstOptimal);
    cmd.copyBufferToImage(buffer, this->getHandle(), vk::ImageLayout::eTransferDstOptimal, regions);
}

vk::Image VulkanImageObject::getHandle() const noexcept
{
    return std::visit([](const auto& img) -> vk::Image {
        if constexpr (std::is_same_v<std::decay_t<decltype(img)>, vk::Image>) { return img; }
        else { return img->getHandle(); }
    }, m_image);
}

std::byte * VulkanImageObject::getMappedMemoryPtr() const noexcept
{
    return std::visit([](const auto& img) -> std::byte * {
        if constexpr (std::is_same_v<std::decay_t<decltype(img)>, vk::Image>) { return nullptr; }
        else { return img->getMappedMemoryPtr(); }
    }, m_image);   
}

vk::ImageView VulkanImageObject::getDefaultView() const
{
    return this->getView(vk::ImageSubresourceRange(this->getAspectFlags(), 0, this->getMipLevelCount(), 0, this->getArrayLayerCount()));
}

vk::ImageView VulkanImageObject::getView(const ImageViewKey & image_view_key) const
{
    auto it = m_view_map.find(image_view_key);
    if (it == m_view_map.end()) {
        return m_view_map.emplace(std::make_pair(image_view_key, this->generateView(image_view_key))).first->second.get();
    }
    return it->second.get();
}

vk::ImageType VulkanImageObject::getImageType() const noexcept
{
    if (m_extent.depth > 1u) { return vk::ImageType::e3D; }
    if (m_extent.height > 1u) { return vk::ImageType::e2D; }
    return vk::ImageType::e1D;
}

bool VulkanImageObject::_create(VulkanContext *context_p, vk::ImageTiling tiling, MemoryAllocationCreateInfo memory_info)
{
    m_context_p = context_p;
    if (m_flags & vk::ImageCreateFlagBits::eCubeCompatible) {
        m_array_layers = 6;
    }
    if (not m_mip_level_count) { //- means has mipmaps
        m_usage |= vk::ImageUsageFlagBits::eTransferSrc;
        m_mip_level_count = std::floor(std::log2(std::max(m_extent.width, m_extent.height))) + 1u;
    }
    vk::ImageCreateInfo image_info;
    image_info.setFlags(m_flags)
        .setImageType(this->getImageType())
        .setFormat(m_format)
        .setExtent(m_extent)
        .setMipLevels(this->getMipLevelCount())
        .setArrayLayers(this->getArrayLayerCount())
        .setSamples(m_samples)
        .setTiling(tiling)
        .setUsage(m_usage)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setSharingMode(vk::SharingMode::eExclusive);
    m_image = m_context_p->getMemoryAllocator().createImage(image_info, memory_info);
    auto interval = LayoutMapInterval::right_open(0, this->getMipLevelCount() * this->getArrayLayerCount());
    m_layout_map.set(std::make_pair(interval, image_info.initialLayout));
    return this->isCreated();
}

vk::UniqueImageView VulkanImageObject::generateView(const ImageViewKey &image_view_key) const
{
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(this->getHandle())
        .setViewType(this->deduceImageViewType(image_view_key.m_range))
        .setComponents(image_view_key.m_components)
        .setFormat(m_format)
        .setSubresourceRange(image_view_key.m_range);
    return m_context_p->getDevice().createImageViewUnique(view_info);   
}

vk::ImageAspectFlags VulkanImageObject::getAspectFlags() const noexcept
{
    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    if (m_format == vk::Format::eD32Sfloat) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
    } else if (m_format == vk::Format::eD24UnormS8Uint or m_format == vk::Format::eD32SfloatS8Uint) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    }
    return aspect_mask;
}

vk::ImageViewType VulkanImageObject::deduceImageViewType(const vk::ImageSubresourceRange &subresource_range) const noexcept
{
    vk::ImageViewType view_type = {};
    switch (this->getImageType()) {
        case vk::ImageType::e1D : {
            view_type = subresource_range.layerCount > 1 ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
        } break;
        case vk::ImageType::e2D : {
            if (m_flags & vk::ImageCreateFlagBits::eCubeCompatible) {
                view_type = subresource_range.layerCount > 6 ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
            } else {
                view_type = subresource_range.layerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
            }
        } break;
        case vk::ImageType::e3D : {
            view_type = vk::ImageViewType::e3D;
        } break;
    }
    return view_type;
}

vk::ImageSubresourceRange VulkanImageObject::getFullResourceRange() const noexcept
{
    return vk::ImageSubresourceRange(this->getAspectFlags(), 0, this->getMipLevelCount(), 0, this->getArrayLayerCount());
}

std::vector<VulkanImageObject::LayoutMapIntervalType> VulkanImageObject::getLayoutIntervals(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept
{
    std::vector<LayoutMapIntervalType> intervals;
    if (base_layer == 0 and layer_count == m_array_layers and base_mip_level == 0 and mip_level_count == m_mip_level_count) {
        intervals.emplace_back(LayoutMapInterval::right_open(0, m_mip_level_count * m_array_layers));
        return intervals;
    }
    for (uint32_t layer = base_layer; layer < base_layer + layer_count; ++layer) {
        auto cur_interval = LayoutMapInterval::right_open(this->getLayoutKey(layer, base_mip_level), this->getLayoutKey(layer, base_mip_level + mip_level_count));
        if (not intervals.empty()) {
            auto & prev_interval = intervals.back();
            if (prev_interval.upper() == cur_interval.lower()) {
                prev_interval = LayoutMapInterval::right_open(prev_interval.lower(), cur_interval.upper());
                continue;
            }
        }
        intervals.emplace_back(cur_interval);
    }
    return intervals;
}

std::optional<vk::ImageLayout> VulkanImageObject::getLayout(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept
{
    std::optional<vk::ImageLayout> layout;
    auto intervals = this->getLayoutIntervals(base_layer, layer_count, base_mip_level, mip_level_count);
    for (auto interval : intervals) {
        auto [begin, end] = m_layout_map.equal_range(interval);
        for (const auto & [equal_interval, wrapped_layout] : std::ranges::subrange(begin, end)) {
            if (not boost::icl::within(interval, equal_interval)) { return std::nullopt; }
            if (not layout) { layout = wrapped_layout.getValue(); }
            else if (layout.value() != wrapped_layout.getValue()) { return std::nullopt; }
        }
    }
    return layout;
}

void VulkanImageObject::blitTo(VulkanCommandBufferObject &cmd,
    const vk::ImageSubresourceLayers &src_subresource,
    const std::pair<vk::Offset3D, vk::Offset3D> &src_offsets,
    VulkanImageObject &dst,
    const vk::ImageSubresourceLayers &dst_subresource,
    const std::pair<vk::Offset3D, vk::Offset3D> & dst_offsets,
    vk::Filter filter)
{
    this->transitLayout(cmd, {this->getAspectFlags(), src_subresource.mipLevel, 1, src_subresource.baseArrayLayer, src_subresource.layerCount}, vk::ImageLayout::eTransferSrcOptimal);
    dst.transitLayout(cmd, {dst.getAspectFlags(), dst_subresource.mipLevel, 1, dst_subresource.baseArrayLayer, dst_subresource.layerCount}, vk::ImageLayout::eTransferDstOptimal);
    vk::ImageBlit2 blit_region2 = {};
    blit_region2.setSrcSubresource(src_subresource)
        .setDstSubresource(dst_subresource);
    memcpy(blit_region2.srcOffsets, &src_offsets, sizeof(blit_region2.srcOffsets));
    memcpy(blit_region2.dstOffsets, &dst_offsets, sizeof(blit_region2.dstOffsets));
    vk::BlitImageInfo2 blit_info = {};
    blit_info.setSrcImage(this->getHandle())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(dst.getHandle())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(blit_region2)
        .setFilter(filter);
    cmd.blitImage2(blit_info);
}

void VulkanImageObject::copyTo(VulkanCommandBufferObject &cmd, const vk::ImageSubresourceLayers &src_subresource, const vk::Offset3D &src_offset, VulkanImageObject &dst, const vk::ImageSubresourceLayers &dst_subresource, const vk::Offset3D &dst_offset, const vk::Extent3D & extent)
{
    this->transitLayout(cmd, vk::ImageLayout::eTransferSrcOptimal);
    dst.transitLayout(cmd, vk::ImageLayout::eTransferDstOptimal);
    vk::ImageCopy2 copy_region2 = {};
    copy_region2.setSrcSubresource(src_subresource)
        .setDstSubresource(dst_subresource)
        .setSrcOffset(src_offset)
        .setDstOffset(dst_offset)
        .setExtent(extent);
    vk::CopyImageInfo2 copy_info = {};
    copy_info.setSrcImage(this->getHandle())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(dst.getHandle())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(copy_region2);
    cmd.copyImage2(copy_info);
}

// - VulkanAttachment

VulkanAttachment::VulkanAttachment(const VulkanImageObject::SharedPointer &image_sp) :
    VulkanAttachment(image_sp, 0, 0, image_sp ? image_sp->getArrayLayerCount() : 1u)
{
}

VulkanAttachment::VulkanAttachment(const VulkanImageObject::SharedPointer &image_sp, uint32_t mip_level, uint32_t layer, uint32_t layer_count) :
    m_image_sp(image_sp),
    m_mip_level(mip_level),
    m_layer(layer),
    m_layer_count(layer_count)
{
    if (not this->isValid()) {
        std::runtime_error error("Invalid image for attachment");
        lcf_log_error(error.what());
        throw error;
    }
    bool is_layer_valid = m_layer + m_layer_count <= m_image_sp->getArrayLayerCount();
    bool is_mip_level_valid = m_mip_level < m_image_sp->getMipLevelCount();
    if (not is_layer_valid or not is_mip_level_valid) {
        std::runtime_error error("Invalid attachment parameters");
        lcf_log_error(error.what());
        throw error;
    }
}

void VulkanAttachment::blitTo(VulkanCommandBufferObject & cmd, VulkanAttachment &dst, vk::Filter filter, const Offset3DPair &src_offsets, const Offset3DPair &dst_offsets)
{
    auto & dst_image = *dst.getImageSharedPointer();
    m_image_sp->blitTo(cmd,
        vk::ImageSubresourceLayers(m_image_sp->getAspectFlags(), m_mip_level, m_layer, m_layer_count),
        src_offsets,
        dst_image,
        vk::ImageSubresourceLayers(dst_image.getAspectFlags(), dst.getMipLevel(), dst.getLayer(), dst.getLayerCount()),
        dst_offsets,
        filter
    );
}

void VulkanAttachment::blitTo(VulkanCommandBufferObject &cmd, VulkanAttachment &dst, vk::Filter filter)
{
    const auto & [sx, sy, sz] = this->getExtent();
    const auto & [dx, dy, dz] = dst.getExtent();
    this->blitTo(cmd, dst, filter,
        {{ 0, 0, 0 }, {static_cast<int32_t>(sx), static_cast<int32_t>(sy), static_cast<int32_t>(sz)}},
        {{ 0, 0, 0 }, {static_cast<int32_t>(dx), static_cast<int32_t>(dy), static_cast<int32_t>(dz)}}
    );
}

void VulkanAttachment::copyTo(VulkanCommandBufferObject & cmd, VulkanAttachment &dst, const vk::Offset3D &src_offset, const vk::Offset3D &dst_offset, const vk::Extent3D &extent)
{
    auto & dst_image = *dst.getImageSharedPointer();
    m_image_sp->copyTo(cmd,
        vk::ImageSubresourceLayers(m_image_sp->getAspectFlags(), m_mip_level, m_layer, m_layer_count),
        src_offset,
        dst_image,
        vk::ImageSubresourceLayers(dst_image.getAspectFlags(), dst.getMipLevel(), dst.getLayer(), dst.getLayerCount()),
        dst_offset,
        extent
    );
}

vk::ImageSubresourceRange VulkanAttachment::getSubresourceRange() const noexcept
{
    return vk::ImageSubresourceRange(m_image_sp->getAspectFlags(), m_mip_level, 1, m_layer, m_layer_count);
}

vk::ImageView VulkanAttachment::getImageView() const noexcept
{
    return m_image_sp->getView(this->getSubresourceRange());
}

void VulkanAttachment::transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout)
{

    m_image_sp->transitLayout(cmd, this->getSubresourceRange(), new_layout);
}

vk::Extent3D VulkanAttachment::getExtent() const noexcept
{
    vk::Extent3D extent = m_image_sp->getExtent();
    return { extent.width >> m_mip_level, extent.height >> m_mip_level, extent.depth };
}
