#pragma once

#include "Renderer.h"
#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorManager.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanFramebuffer.h"

namespace lcf {
    using namespace lcf::render;

    class VulkanRenderer : public Renderer
    {
    public:
        VulkanRenderer(VulkanContext * context);
        ~VulkanRenderer();
        void setRenderTarget(render::RenderTarget * target) override;
        void create() override;
        bool isValid() const override { return m_context and m_context->isValid() and m_render_target and m_render_target->isValid(); }
        void render() override;
    private:
        VulkanContext * m_context;
        VulkanSwapchain * m_render_target; // temporary
        struct FrameResources
        {
            FrameResources() = default;
            vk::UniqueFence frame_ready;
            vk::UniqueSemaphore target_available;
            vk::UniqueSemaphore render_finished;
            vk::CommandBuffer command_buffer;
            VulkanDescriptorManager descriptor_manager;
            // temporary
            VulkanFramebuffer::UniquePointer framebuffer;
            bool is_render_initiated = true;
        };
        std::vector<FrameResources> m_frame_resources;
        uint32_t m_current_frame_index = 0;

        //! temporary
        VulkanPipeline::UniquePointer m_compute_pipeline;
        VulkanPipeline::UniquePointer m_graphics_pipeline;
        VulkanBuffer::UniquePointer m_vertext_buffer;
        VulkanBuffer::UniquePointer m_index_buffer;

        VulkanImage::UniquePointer m_texture_image;
        vk::UniqueSampler m_texture_sampler;
    };
}