#pragma once

#include "VulkanBufferWriter.h"
#include "VulkanBufferObject2.h"
#include <vector>

namespace lcf::render {
    class VulkanContext;

    class VulkanCommandBufferObject;

    class VulkanBufferObjectGroup
    {
        using Self = VulkanBufferObjectGroup;
        using BufferList = std::vector<VulkanBufferObject2>;
    public:
        VulkanBufferObjectGroup() = default;
        ~VulkanBufferObjectGroup() = default;
        VulkanBufferObjectGroup(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBufferObjectGroup(Self &&) = default;
        Self & operator=(Self &&) = default;
        VulkanBufferObject2 & operator[](size_t index) noexcept { return m_buffer_object_list.at(index); }
        const VulkanBufferObject2 & operator[](size_t index) const noexcept { return m_buffer_object_list.at(index); }
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