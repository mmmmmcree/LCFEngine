#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanPipeline.h"
#include "VulkanImageObject.h"
#include "VulkanFramebufferObject.h"
#include "Entity.h"
#include "VulkanCommandBufferObject.h"
#include "VulkanBufferObject.h"
#include "VulkanBufferObject2.h"
#include "VulkanBufferObjectGroup.h"
#include "VulkanMesh.h"
#include "VulkanSampler.h"
#include "VulkanMaterial.h"
#include "VulkanDescriptorSet.h"

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
        void render(const Entity & camera, const Entity & render_target);
    private:
        VulkanContext * m_context_p;
        struct FrameResources
        {
            FrameResources() = default;
            VulkanCommandBufferObject command_buffer;
            VulkanCommandBufferObject data_transfer_command_buffer; // dependency of command_buffer
            // temporary
            VulkanFramebufferObject fbo;
        };
        std::vector<FrameResources> m_frame_resources;
        uint32_t m_current_frame_index = 0;

        //! temporary
        VulkanDescriptorSet m_per_view_descriptor_set;
        VulkanDescriptorSet m_per_renderable_descriptor_set;

        VulkanPipeline m_compute_pipeline;
        VulkanPipeline m_graphics_pipeline;
        VulkanPipeline m_skybox_pipeline;

        VulkanBufferObject2 m_per_view_uniform_buffer;

        VulkanBufferObject2 m_indirect_call_buffer;
        
        VulkanBufferObjectGroup m_per_renderable_ssbo_group;
        
        VulkanMesh m_mesh;
        VulkanMaterial m_material;
        VulkanBufferObject::SharedPointer m_per_material_params_ssbo_sp; //- ssbo that store params_buffer's address
        VulkanBufferObject2 m_material_params; //- params with real data
        VulkanMaterial m_skybox_material;
    };
}