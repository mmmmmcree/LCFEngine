#pragma once

#include "VulkanBufferController.h"
#include "VulkanBufferProxy.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanBufferGroupObject
    {
        using Self = VulkanBufferGroupObject;
    public:
        class BufferObject : public STDPointerDefs<BufferObject>
        {
            using Self = BufferObject;
        public:
            BufferObject() = default;
            ~BufferObject() = default;
            BufferObject(const Self &) = delete;
            Self & operator=(const Self &) = delete;
            BufferObject(Self &&) = default;
            Self & operator=(Self &&) = default;
        private:
            VulkanBufferGroupObject * m_group_p;
            VulkanBufferProxy m_buffer_proxy;
            BufferWriteSegments m_write_segments;
        };
    public:
        VulkanBufferGroupObject() = default;
        ~VulkanBufferGroupObject() = default;
        VulkanBufferGroupObject(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBufferGroupObject(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
    private:
        VulkanBufferController m_buffer_controller;
        GPUBufferUsage m_usage = GPUBufferUsage::eUndefined;
        std::vector<BufferObject> m_buffer_objects;
    };
}