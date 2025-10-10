#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorManager.h"
#include "VulkanPipeline.h"
#include "VulkanImage.h"
#include "VulkanFramebufferObject.h"
#include "Entity.h"
#include "VulkanTimelineSemaphore.h"
#include "VulkanCommandBufferObject.h"
#include "VulkanBufferObject.h"
#include "VulkanMesh.h"

namespace lcf {
    using namespace lcf::render;

    class VulkanRenderer
    {
    public:
        VulkanRenderer(VulkanContext * context);
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        ~VulkanRenderer();
        void setRenderTarget(const RenderTarget::SharedPointer & render_target);
        void create();
        void render(const Entity & camera);
    private:
        VulkanContext * m_context_p;
        VulkanSwapchain::WeakPointer m_render_target;
        struct FrameResources
        {
            FrameResources() = default;
            vk::UniqueSemaphore render_finished;
            VulkanCommandBufferObject command_buffer;
            VulkanDescriptorManager descriptor_manager;
            // temporary
            VulkanFramebufferObject fbo;
        };
        std::vector<FrameResources> m_frame_resources;
        uint32_t m_current_frame_index = 0;

        //! temporary
        VulkanDescriptorManager m_global_descriptor_manager;
        vk::DescriptorSet m_per_view_descriptor_set;
        vk::DescriptorSet m_per_renderable_descriptor_set;

        VulkanPipeline m_compute_pipeline;
        VulkanPipeline m_graphics_pipeline;

        VulkanBufferObject m_per_view_uniform_buffer;

        VulkanBufferObject m_indirect_call_buffer;

        VulkanMesh m_mesh;
        
        VulkanBufferObject m_per_renderable_vertex_buffer;
        VulkanBufferObject m_per_renderable_index_buffer;
        VulkanBufferObject m_per_renderable_transform_buffer;

        VulkanImage::SharedPointer m_cube_map;
        VulkanImage::UniquePointer m_texture_image;
        vk::UniqueSampler m_texture_sampler;
    };
}