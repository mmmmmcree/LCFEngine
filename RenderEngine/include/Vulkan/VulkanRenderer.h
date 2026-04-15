#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanPipeline.h"
#include "memory/VulkanImageObject.h"
#include "VulkanFramebufferObject.h"
#include "ecs/Entity.h"
#include "VulkanCommandBufferObject.h"
#include "memory/VulkanBufferObject.h"
#include "memory/VulkanBufferObjectGroup.h"
#include "VulkanMesh.h"
#include "VulkanSampler.h"
#include "VulkanDescriptorSet.h"
#include <unordered_map>

namespace lcf {
namespace ecf {
    class Registry;
}
    using namespace lcf::render;

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        ~VulkanRenderer();
        void create(VulkanContext * context_p, const std::pair<uint32_t, uint32_t> & max_extent, ecs::Registry & registry);
        void render(const ecs::Entity & camera, const ecs::Entity & render_target);
    private:
        VulkanContext * m_context_p;
        ecs::Registry * m_registry_p;
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

        VulkanPipeline m_compute_pipeline;
        VulkanPipeline m_graphics_pipeline;
        VulkanPipeline m_skybox_pipeline;

        VulkanBufferObject m_per_view_uniform_buffer;

        VulkanBufferObject m_indirect_call_buffer;
        
        VulkanBufferObjectGroup m_per_renderable_ssbo_group;
        
        VulkanMesh m_mesh;
        std::vector<VulkanMesh> m_meshes;
        std::vector<VulkanBufferObject> m_material_params_list;
        std::vector<VulkanBufferObject> m_material_texture_ids_list;
    };
}