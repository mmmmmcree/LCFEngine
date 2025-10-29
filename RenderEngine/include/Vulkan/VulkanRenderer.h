#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorManager.h"
#include "VulkanPipeline.h"
#include "VulkanImage.h"
#include "VulkanFramebufferObject.h"
#include "Entity.h"
#include "VulkanCommandBufferObject.h"
#include "VulkanBufferObject.h"
#include "VulkanMesh.h"
#include "VulkanSampler.h"
#include "VulkanMaterial.h"

namespace lcf {
    using namespace lcf::render;

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        ~VulkanRenderer();
        void create(VulkanContext * context_p, const std::pair<uint32_t, uint32_t> & max_extent);
        void render(const Entity & camera, RenderTarget::WeakPointer render_target_wp);
    private:
        VulkanContext * m_context_p;
        struct FrameResources
        {
            FrameResources() = default;
            VulkanCommandBufferObject command_buffer;
            VulkanDescriptorManager descriptor_manager;
            // temporary
            VulkanFramebufferObject fbo;
        };
        std::vector<FrameResources> m_frame_resources;
        uint32_t m_current_frame_index = 0;

        //! temporary
        vk::DescriptorSet m_per_view_descriptor_set;
        vk::DescriptorSet m_per_renderable_descriptor_set;

        VulkanPipeline m_compute_pipeline;
        VulkanPipeline m_graphics_pipeline;
        VulkanPipeline m_skybox_pipeline;

        VulkanBufferObject m_per_view_uniform_buffer;

        VulkanBufferObject m_indirect_call_buffer;

        VulkanMesh m_mesh;
        
        VulkanBufferObject m_per_renderable_vertex_buffer;
        VulkanBufferObject m_per_renderable_index_buffer;
        VulkanBufferObject m_per_renderable_transform_buffer;

        VulkanMaterial m_material;
        VulkanMaterial m_skybox_material;
    };
}