#include "Vulkan/VulkanRenderer.h"
#include "Vulkan/VulkanShaderProgram.h"
#include "Vulkan/vulkan_utililtie.h"
#include "Vulkan/vulkan_constants.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "ecs/Entity.h"
#include "Transform.h"
#include <boost/container/small_vector.hpp>
#include "bytes.h"
#include "render_assets/ModelLoader.h"
#include "render_assets/Texture2D.h"
#include "common/glsl_type_traits.h"
#include "Vulkan/vulkan_enums.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

lcf::VulkanRenderer::~VulkanRenderer()
{
    auto device = m_context_p->getDevice();
    device.waitIdle();
}

void lcf::VulkanRenderer::create(VulkanContext * context_p, const std::pair<uint32_t, uint32_t> & max_extent)
{
    m_context_p = context_p;
    const auto & sampler_manager = context_p->getSamplerManager();
    m_frame_resources.resize(3); //todo remove constant
    auto device = m_context_p->getDevice();

    auto [max_width, max_height] = max_extent;

    VulkanShaderProgram::SharedPointer compute_shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    compute_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eCompute, "assets/shaders/gradient.comp");
    compute_shader_program->link();

    ComputePipelineCreateInfo compute_pipeline_info(compute_shader_program);
    m_compute_pipeline.create(m_context_p, compute_pipeline_info);

    VulkanShaderProgram::SharedPointer shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex, "assets/shaders/vertex_buffer_test.vert")
        .addShaderFromGlslFile(ShaderTypeFlagBits::eFragment, "assets/shaders/vertex_buffer_test.frag")
        .link();
    GraphicPipelineCreateInfo graphic_pipeline_info;
    graphic_pipeline_info.setShaderProgram(shader_program)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
        .setRasterizationSamples(vk::SampleCountFlagBits::e4)
        .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat);
    m_graphics_pipeline.create(m_context_p, graphic_pipeline_info);

    for (auto &resources : m_frame_resources) {
        resources.command_buffer.create(m_context_p, vk::QueueFlagBits::eGraphics);
        resources.data_transfer_command_buffer.create(m_context_p, vk::QueueFlagBits::eGraphics);

        VulkanFramebufferObjectCreateInfo fbo_info;
        fbo_info.setMaxExtent({static_cast<uint32_t>(max_width), static_cast<uint32_t>(max_height)})
            .addColorFormats(graphic_pipeline_info.getColorAttachmentFormats())
            .setSampleCount(graphic_pipeline_info.getRasterizationSamples())
            .setDepthStencilFormat(graphic_pipeline_info.getDepthAttachmentFormat())
            .setResolveMode(vk::ResolveModeFlagBits::eAverage);
        resources.fbo.create(m_context_p, fbo_info);
    }
    // ! temporary

    m_per_view_uniform_buffer.setUsage(GPUBufferUsage::eUniform)
        .create(m_context_p, size_of_v<Matrix4x4> * 3); // projection, view, projection_view

    m_per_renderable_ssbo_group.create(m_context_p, GPUBufferPattern::eDynamic);
    m_per_renderable_ssbo_group.emplace(100 * size_of_v<vk::DeviceAddress>, GPUBufferUsage::eShaderStorage); // vertex buffer addresses
    m_per_renderable_ssbo_group.emplace(100 * size_of_v<vk::DeviceAddress>, GPUBufferUsage::eShaderStorage); // index buffer addresses
    m_per_renderable_ssbo_group.emplace(200 * size_of_v<Matrix4x4>, GPUBufferUsage::eShaderStorage); // transforms
    m_per_renderable_ssbo_group.emplace(size_of_v<vk::DeviceAddress> * 2 * 100, GPUBufferUsage::eShaderStorage); // material records

    // m_per_material_params_ssbo_sp = VulkanBufferObject::makeShared();
    // m_per_material_params_ssbo_sp->setUsage(GPUBufferUsage::eShaderStorage)
    //     .create(m_context_p, size_of_v<vk::DeviceAddress> * 2 * 100);

    m_indirect_call_buffer.setUsage(GPUBufferUsage::eIndirect)
        .create(m_context_p, size_of_v<vk::DrawIndirectCommand> + 1 * size_of_v<vk::DrawIndirectCommand>); // uint32_t(for IndirectDrawCount) + padding | vk::DrawIndirectCommand ...

    auto per_view_descriptor_set_layout_sp = VulkanDescriptorSetLayout::makeShared();
    per_view_descriptor_set_layout_sp->setBindings(vkconstants::per_view_bindings)
        .setIndex(std::to_underlying(DescriptorSetBindingPoints::ePerView))
        .create(m_context_p);
    auto per_renderable_descriptor_set_layout_sp = VulkanDescriptorSetLayout::makeShared();
    per_renderable_descriptor_set_layout_sp->setBindings(vkconstants::per_renderable_bindings)
        .setIndex(std::to_underlying(DescriptorSetBindingPoints::ePerRenderable))
        .create(m_context_p);
    m_per_view_descriptor_set.create(per_view_descriptor_set_layout_sp);
    m_per_renderable_descriptor_set.create(per_renderable_descriptor_set_layout_sp);

    vk::DescriptorBufferInfo per_view_buffer_info;
    per_view_buffer_info.setBuffer(m_per_view_uniform_buffer.getHandle())
        .setOffset(0)
        .setRange(m_per_view_uniform_buffer.getSizeInBytes());
    auto ds_updater = m_per_view_descriptor_set.generateUpdater();
    ds_updater.add(std::to_underlying(PerViewBindingPoints::eCamera), per_view_buffer_info)
        .update();

    ds_updater = m_per_renderable_descriptor_set.generateUpdater();
    ds_updater.add(std::to_underlying(PerRenderableBindingPoints::eVertexBuffer), m_per_renderable_ssbo_group[0].generateBufferInfo())
        .add(std::to_underlying(PerRenderableBindingPoints::eIndexBuffer), m_per_renderable_ssbo_group[1].generateBufferInfo())
        .add(std::to_underlying(PerRenderableBindingPoints::eTransform), m_per_renderable_ssbo_group[2].generateBufferInfo())
        .add(vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eMaterialRecords), m_per_renderable_ssbo_group[3].generateBufferInfo())
        .update();

    auto image1_sp = Texture2D::makeShared();
    image1_sp->loadFromFileGpuFriendly({"assets/images/bk.jpg"});
    auto image2_sp = Texture2D::makeShared();
    image2_sp->loadFromFileGpuFriendly({"assets/images/qt256.png"});

    // auto load_mode_result = ModelLoader {}.load("./assets/models/dinosaur/source/Rampaging T-Rex.glb");
    // auto load_mode_result = ModelLoader {}.load("./assets/models/BarbieDodgePickup/scene.gltf");
    static uint32_t s_texture_id = 0;
    std::unordered_map<Texture2D::SharedPointer, uint32_t> texture_id_map;
    auto upload_model_to_gpu = [this, &texture_id_map] (VulkanCommandBufferObject & cmd, Model & model) {
        auto geometry_range = model.m_render_primitive_list |
            stdv::transform([](const auto & render_primitive) -> const Geometry & { return render_primitive.getGeometry(); });
        for (const auto & geometry: geometry_range) {
            auto & mesh = m_meshes.emplace_back();
            mesh.create(m_context_p, cmd,
                generate_interleaved_segments<glsl::std140::enum_value_type_mapping_t>(
                    geometry,
                    VertexAttributeFlags::ePosition | VertexAttributeFlags::eNormal | VertexAttributeFlags::eTexCoord0
                ), geometry.getIndices());
        }

        auto material_range = model.m_render_primitive_list |
            stdv::transform([](const auto & render_primitive) -> const Material & { return render_primitive.getMaterial(); });
        for (const auto & material: material_range) {
            auto & texture_params_buffer = m_material_params_list.emplace_back();
            texture_params_buffer.setUsage(GPUBufferUsage::eShaderStorage)
                .setPattern(GPUBufferPattern::eStatic)
                .create(m_context_p, 1);
            texture_params_buffer.appendWriteSegments(generate_interleaved_segments<glsl::std140::enum_value_type_mapping_t>(material, ShadingModel::eStandard));
            auto & texture_ids_buffer = m_material_texture_ids_list.emplace_back();
            texture_ids_buffer.setUsage(GPUBufferUsage::eShaderStorage)
                .setPattern(GPUBufferPattern::eStatic)
                .create(m_context_p, 1);
            for (auto texture_resource : get_texture_resources(material, ShadingModel::eStandard)) {
                if (texture_id_map.contains(texture_resource)) { continue; }
                uint32_t texture_id = s_texture_id++;
                texture_id_map[texture_resource] = texture_id;
                auto & image_sp = m_texture_map[texture_id] = VulkanImageObject::makeShared();
                image_sp->setFormat(enum_cast<vk::Format>(texture_resource->getDecodeFormat()))
                    .setExtent({ texture_resource->getWidth(), texture_resource->getHeight(), 1u })
                    .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
                    .create(m_context_p);
                image_sp->setData(cmd, texture_resource->getDataSpan());
            }
            for (uint32_t offset = 0; const auto & texture_resource : get_texture_resources(material, ShadingModel::eStandard)) {
                texture_ids_buffer.addWriteSegment({as_bytes_from_value(texture_id_map[texture_resource]), offset});
                offset += sizeof(uint32_t);
            }
            texture_params_buffer.commit(cmd);
            texture_ids_buffer.commit(cmd);
        }
    };

    auto load_mode_result = ModelLoader {}.load("./assets/models/ToyCar/glTF/ToyCar.gltf");
    vkutils::immediate_submit(m_context_p, vk::QueueFlagBits::eTransfer, [&model = load_mode_result.value(), &upload_model_to_gpu] (VulkanCommandBufferObject & cmd) {
        upload_model_to_gpu(cmd, model);
    });
    load_mode_result = ModelLoader {}.load("./assets/models/DamagedHelmet/DamagedHelmet.gltf");
    vkutils::immediate_submit(m_context_p, vk::QueueFlagBits::eTransfer, [&model = load_mode_result.value(), &upload_model_to_gpu] (VulkanCommandBufferObject & cmd) {
        upload_model_to_gpu(cmd, model);
    });
    
    VulkanImageObject::SharedPointer cube_map_sp;
    VulkanImageObject::SharedPointer texture1_sp;
    VulkanImageObject::SharedPointer texture2_sp;
    VulkanSampler::SharedPointer sampler_sp;

    texture1_sp = VulkanImageObject::makeShared();
    texture1_sp->setFormat(enum_cast<vk::Format>(image1_sp->getDecodeFormat()))
        .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
        .setMipmapped(true)
        .setExtent({ image1_sp->getWidth(), image1_sp->getHeight(), 1u })
        .create(m_context_p);
    texture2_sp = VulkanImageObject::makeShared();
    texture2_sp->setFormat(enum_cast<vk::Format>(image2_sp->getDecodeFormat()))
        .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
        .setMipmapped(true)
        .setExtent({ image2_sp->getWidth(), image2_sp->getHeight(), 1u })
        .create(m_context_p);

    uint32_t cube_width = 1024;
    cube_map_sp = VulkanImageObject::makeShared();
    cube_map_sp->addImageFlags(vk::ImageCreateFlagBits::eCubeCompatible)
        .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst  | vk::ImageUsageFlagBits::eColorAttachment)
        .setFormat(vk::Format::eR8G8B8A8Unorm)
        .setExtent({ cube_width, cube_width, 1u })
        .create(m_context_p);

    VulkanShaderProgram::SharedPointer stc_shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    stc_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex, "assets/shaders/sphere_to_cube.vert")
        .addShaderFromGlslFile(ShaderTypeFlagBits::eGeometry, "assets/shaders/sphere_to_cube.geom")
        .addShaderFromGlslFile(ShaderTypeFlagBits::eFragment, "assets/shaders/sphere_to_cube.frag")
        .link();
    GraphicPipelineCreateInfo stc_pipeline_info;
    stc_pipeline_info.setShaderProgram(stc_shader_program)
        .addColorAttachmentFormat(vk::Format::eR8G8B8A8Unorm);
    VulkanPipeline stc_pipeline;
    stc_pipeline.create(m_context_p, stc_pipeline_info);

    vkutils::immediate_submit(m_context_p, vk::QueueFlagBits::eGraphics, [&](VulkanCommandBufferObject & cmd) {
        texture1_sp->setData(cmd, image1_sp->getDataSpan());
        texture1_sp->generateMipmaps(cmd);

        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(*texture1_sp->getLayout())
            .setImageView(texture1_sp->getDefaultView())
            .setSampler(sampler_manager.getShared(SamplerPreset::eEnvironmentMap)->getHandle());

        const auto & layout_sp = stc_pipeline.getDescriptorSetLayoutSharedPtr(0);
        auto descriptor_set_rp = make_resource_ptr<VulkanDescriptorSet>();
        descriptor_set_rp->create(layout_sp);
        auto descriptor_set_updater = descriptor_set_rp->generateUpdater();
        descriptor_set_updater.add(0, image_info).update();


        auto [w, h, z] = cube_map_sp->getExtent();
        VulkanFramebufferObjectCreateInfo fbo_info;
        fbo_info.setMaxExtent({w, h});
        VulkanFramebufferObject fbo;
        fbo.addColorAttachment(cube_map_sp)
            .create(m_context_p, fbo_info);
        cmd.acquireResourceLease(descriptor_set_rp.lease());
        cmd.bindPipeline(stc_pipeline.getType(), stc_pipeline.getHandle());
        cmd.bindDescriptorSets(stc_pipeline.getType(), stc_pipeline.getPipelineLayout(), descriptor_set_rp->getIndex(), descriptor_set_rp->getHandle(), nullptr);
        fbo.setViewportAndScissor(cmd);
        fbo.beginRendering(cmd);
        cmd.draw(36, 1, 0, 0); // draw with const data in shader program
        fbo.endRendering(cmd);
        cube_map_sp->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

        texture2_sp->setData(cmd, image2_sp->getDataSpan());
        texture2_sp->generateMipmaps(cmd);
    });

    auto skybox_shader_program = VulkanShaderProgram::makeShared(m_context_p);
    skybox_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex, "assets/shaders/skybox.vert")
        .addShaderFromGlslFile(ShaderTypeFlagBits::eFragment, "assets/shaders/skybox.frag")
        .link();
    GraphicPipelineCreateInfo skybox_pipeline_info;
    skybox_pipeline_info.setShaderProgram(skybox_shader_program)
        .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
        .setRasterizationSamples(vk::SampleCountFlagBits::e4)
        .setDepthWriteEnabled(false)
        .setDepthCompareOp(vk::CompareOp::eLessOrEqual)
        .setCullMode(vk::CullModeFlagBits::eFront)
        .setFrontFace(vk::FrontFace::eCounterClockwise);
    m_skybox_pipeline.create(m_context_p, skybox_pipeline_info);

    auto skybox_ds_sp = VulkanDescriptorSet::makeShared();
    skybox_ds_sp->create(m_skybox_pipeline.getDescriptorSetLayoutSharedPtr(std::to_underlying(DescriptorSetBindingPoints::ePerMaterial)));
    m_skybox_material.create(skybox_ds_sp);
    m_skybox_material.setTexture(1, 0, cube_map_sp)
        .setSampler(1, 0, sampler_manager.getShared(SamplerPreset::eEnvironmentMap))
        .commitUpdate();

    auto layout_sp = m_graphics_pipeline.getDescriptorSetLayoutSharedPtr(std::to_underlying(DescriptorSetBindingPoints::ePerMaterial));
    auto material_ds_sp = VulkanDescriptorSet::makeShared();
    material_ds_sp->create(layout_sp);
    m_material.create(material_ds_sp);
    for (const auto & [texture_id, texture_sp] : m_texture_map) {
        m_material.setTexture(1, texture_id, texture_sp);
    }
    m_material.setSampler(0, 0, sampler_manager.getShared(SamplerPreset::eColorMap))
        .setSampler(0, 1, sampler_manager.getShared(SamplerPreset::eColorMap))
        // .setParamsSSBO(0, m_per_material_params_ssbo_sp)
        .commitUpdate();
}

void lcf::VulkanRenderer::render(const ecs::Entity & camera, const ecs::Entity & render_target)
{
    auto device = m_context_p->getDevice();
    auto &current_frame_resources = m_frame_resources[m_current_frame_index];

    VulkanCommandBufferObject & cmd = current_frame_resources.command_buffer; 
    cmd.waitUntilAvailable();

    auto render_target_wp = render_target.getComponent<VulkanSwapchain::WeakPointer>();
    if (render_target_wp.expired()) { return; }
    const auto & render_target_sp = render_target_wp.lock();
    if (not render_target_sp->prepareForRender()) { return; }

    auto [width, height] = render_target_sp->getExtent();
    vk::Viewport viewport;
    viewport. setX(0.0f).setY(height)
        .setWidth(static_cast<float>(width))
        .setHeight(-static_cast<float>(height))
        .setMinDepth(0.0f).setMaxDepth(1.0f);
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 }).setExtent({ width, height });
    
    cmd.begin(vk::CommandBufferBeginInfo{});
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);
    /*
    ! DescriptorSet和Buffer都是command buffer的Resource，需要允许cmd.acquire(resource)
    ! 由cmd控制资源生命周期
    */

    auto &current_framebuffer = current_frame_resources.fbo;

    Matrix4x4 projection, projection_view;
    projection.perspectiveRH_ZO(60.0f, static_cast<float>(width) / height, 0.1f, 1000.0f);
    auto &camera_transform = camera.getComponent<Transform>();
    const auto & camera_view = camera.getComponent<TransformInvertedWorldMatrix>();
    projection_view = projection * camera_view.getMatrix();
    m_per_view_uniform_buffer.addWriteSegment({as_bytes_from_value(projection), 0u})
        .addWriteSegment({as_bytes_from_value(camera_view.getMatrix()), sizeof(Matrix4x4)})
        .addWriteSegment({as_bytes_from_value(projection_view), 2 * sizeof(Matrix4x4)});
    static uint64_t frame_count = 0;
    ++frame_count;
    float angle = 0.1f * frame_count;
    std::vector<Matrix4x4> model_matrices(120);
    for (uint32_t i = 0; i < model_matrices.size(); i += 2) {
        model_matrices[i].rotateAroundSelf(Quaternion::fromAxisAndAngle({1.0f, 1.0f, 0.0f}, angle));
        model_matrices[i + 1].rotateAroundSelf(Quaternion::fromAxisAndAngle({1.0f, 0.0f, 0.0f}, 90.0f));
        model_matrices[i + 1].translateLocalX(10.0f);
    }

    auto & per_renderable_vertex_buffer_ssbo = m_per_renderable_ssbo_group[0];
    auto & per_renderable_index_buffer_ssbo = m_per_renderable_ssbo_group[1];
    auto & per_renderable_transform_ssbo = m_per_renderable_ssbo_group[2];
    auto & per_renderable_material_records = m_per_renderable_ssbo_group[3];

    for (uint32_t i = 0; i < m_meshes.size(); ++i) {
        const auto & vertex_buffer_address = m_meshes[i].getVertexBufferAddress();
        const auto & index_buffer_address = m_meshes[i].getIndexBufferAddress();
        size_t offset = i * size_of_v<vk::DeviceAddress>;
        per_renderable_vertex_buffer_ssbo.addWriteSegment({as_bytes_from_value(vertex_buffer_address), offset});
        per_renderable_index_buffer_ssbo.addWriteSegment({as_bytes_from_value(index_buffer_address), offset});
    }
    per_renderable_transform_ssbo.addWriteSegment({as_bytes(model_matrices), 0u});

    uint32_t instance_count = 0;
    std::vector<vk::DrawIndirectCommand> indirect_calls(m_meshes.size());
    for (int i = 0; i < indirect_calls.size(); ++i) {
        /**
         * @brief 
         * indiret call index: draw index, use as geometry index to locate vertex buffer and index buffer
         * firstInstance: instance index, use as offset to locate model matrix, one for each instance
         */
        auto & indirect_call = indirect_calls[i];
        const auto & mesh = m_meshes[i];
        indirect_call.setFirstVertex(0)
            .setVertexCount(mesh.getIndexCount())
            .setFirstInstance(instance_count)
            .setInstanceCount(2);
        instance_count += indirect_call.instanceCount;
    }
    for (int i = 0; i < indirect_calls.size(); ++i) {
        const auto & material_params_buffer = m_material_params_list[i];
        const auto & texture_ids_buffer = m_material_texture_ids_list[i];
        size_t params_offset = 2 * i * size_of_v<vk::DeviceAddress>;
        size_t texture_ids_offset = params_offset + size_of_v<vk::DeviceAddress>;
        per_renderable_material_records.addWriteSegment({as_bytes_from_value(material_params_buffer.getDeviceAddress()), params_offset})
            .addWriteSegment({as_bytes_from_value(texture_ids_buffer.getDeviceAddress()), texture_ids_offset});
    }

    uint32_t indirect_call_count = static_cast<uint32_t>(indirect_calls.size());
    m_indirect_call_buffer.addWriteSegment({as_bytes_from_value(indirect_call_count), 0})
        .addWriteSegment({as_bytes(indirect_calls), size_of_v<vk::DrawIndirectCommand>});

    m_indirect_call_buffer.commit(cmd);
    m_per_view_uniform_buffer.commit(cmd);
    m_per_renderable_ssbo_group.commitAll(cmd);

    current_framebuffer.beginRendering(cmd);

    uint32_t dynamic_offset = 0;
    m_graphics_pipeline.bind(cmd);
    m_graphics_pipeline.bindDescriptorSet(cmd, m_per_view_descriptor_set, dynamic_offset);
    m_graphics_pipeline.bindDescriptorSet(cmd, m_per_renderable_descriptor_set);
    m_graphics_pipeline.bindDescriptorSet(cmd, m_material.getDescriptorSet());
    
    cmd.drawIndirectCount(m_indirect_call_buffer.getHandle(), sizeof(vk::DrawIndirectCommand),
        m_indirect_call_buffer.getHandle(), 0,
        indirect_call_count, sizeof(vk::DrawIndirectCommand));
    
    m_skybox_pipeline.bind(cmd);

    m_skybox_pipeline.bindDescriptorSet(cmd, m_per_view_descriptor_set, dynamic_offset);
    m_skybox_pipeline.bindDescriptorSet(cmd, m_skybox_material.getDescriptorSet());
    cmd.draw(36, 1, 0, 0); // draw with const data in shader program
    
    current_framebuffer.endRendering(cmd);

    auto & render_target_resources = render_target_sp->getCurrentFrameResources();
    auto & target_image_sp = render_target_resources.getImageSharedPointer();
    // blit to target
    auto msaa_attachment = current_framebuffer.getMSAAResolveAttachment();
    if (msaa_attachment) {
        VulkanAttachment target_attachment(target_image_sp);
        auto [w, h, z] = target_image_sp->getExtent();
        msaa_attachment->blitTo(cmd, target_attachment, vk::Filter::eLinear,
            {{0, 0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height), 1}},
            {{0, 0, 0}, {static_cast<int32_t>(w), static_cast<int32_t>(h), 1}});
    }

    target_image_sp->transitLayout(cmd, vk::ImageLayout::ePresentSrcKHR);
    cmd.end();

    vk::SemaphoreSubmitInfo wait_info;
    wait_info.setSemaphore(render_target_resources.getTargetAvailableSemaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    cmd.addWaitSubmitInfo(wait_info)
        // .addWaitSubmitInfo(data_transfer_complete_info)
        .addSignalSubmitInfo(render_target_resources.getPresentReadySemaphore());
    cmd.submit();

    render_target_sp->finishRender();
    ++m_current_frame_index %= m_frame_resources.size();
}
