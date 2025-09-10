#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorManager.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanFramebuffer.h"
#include "Entity.h"
#include "VulkanTimelineSemaphore.h"
#include "VulkanCommandBuffer.h"

namespace lcf {
    using namespace lcf::render;

    class VulkanRenderer
    {
    public:
        VulkanRenderer(VulkanContext * context);
        ~VulkanRenderer();
        void setRenderTarget(const RenderTarget::SharedPointer & render_target);
        void setCamera(const Entity & camera_entity);
        void create();
        void render();
    private:
        VulkanContext * m_context_p;
        VulkanSwapchain::WeakPointer m_render_target;
        struct FrameResources
        {
            FrameResources() = default;
            vk::UniqueSemaphore render_finished;
            VulkanCommandBuffer command_buffer;
            VulkanDescriptorManager descriptor_manager;
            // temporary
            VulkanFramebuffer::UniquePointer framebuffer;
        };
        VulkanTimelineBuffer::UniquePointer m_global_uniform_buffer;
        std::vector<FrameResources> m_frame_resources;
        uint32_t m_current_frame_index = 0;

        //! temporary
        VulkanPipeline::UniquePointer m_compute_pipeline;
        VulkanPipeline::UniquePointer m_graphics_pipeline;
        VulkanBuffer::UniquePointer m_vertext_buffer;
        VulkanBuffer::UniquePointer m_index_buffer;

        VulkanBuffer::UniquePointer m_descriptor_buffer;

        VulkanImage::UniquePointer m_texture_image;
        vk::UniqueSampler m_texture_sampler;
        Entity m_camera_entity;
    };
}