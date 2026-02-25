#pragma once

#include "vulkan_fwd_decls.h"
#include "VulkanBufferWriter.h"
#include "VulkanBufferObject.h"
#include <vector>

namespace lcf::render {
    class VulkanBufferObjectGroup
    {
        using Self = VulkanBufferObjectGroup;
        using BufferList = std::vector<VulkanBufferObject>;
    public:
        VulkanBufferObjectGroup() = default;
        ~VulkanBufferObjectGroup() noexcept = default;
        VulkanBufferObjectGroup(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBufferObjectGroup(Self &&) = default;
        Self & operator=(Self &&) = default;
        VulkanBufferObject & operator[](size_t index) noexcept { return m_buffer_object_list.at(index); }
        const VulkanBufferObject & operator[](size_t index) const noexcept { return m_buffer_object_list.at(index); }
    public:
        bool create(VulkanContext * context_p, GPUBufferPattern pattern);
        void emplace(uint64_t size, GPUBufferUsage buffer_usage);
        void commitAll(VulkanCommandBufferObject & cmd) noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanBufferWriter m_buffer_writer;
        BufferList m_buffer_object_list;
    };
}