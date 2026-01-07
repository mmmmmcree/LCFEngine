#include "Vulkan/VulkanRenderer.h"
#include "Vulkan/VulkanShaderProgram.h"
#include "Vulkan/vulkan_utililtie.h"
#include "Vulkan/vulkan_constants.h"
#include "Matrix.h"
#include "common/geometry_data.h"
#include "Quaternion.h"
#include "Image/Image.h"
#include "Entity.h"
#include "Transform.h"
#include <boost/container/small_vector.hpp>
#include "bytes.h"
#include "ModelLoader.h"

lcf::VulkanRenderer::~VulkanRenderer()
{
    auto device = m_context_p->getDevice();
    device.waitIdle();
}

void lcf::VulkanRenderer::create(VulkanContext * context_p, const std::pair<uint32_t, uint32_t> & max_extent)
{
    m_context_p = context_p;
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
    m_per_renderable_ssbo_group.emplace(size_of_v<vk::DeviceAddress>, GPUBufferUsage::eShaderStorage); // vertex buffer
    m_per_renderable_ssbo_group.emplace(size_of_v<vk::DeviceAddress>, GPUBufferUsage::eShaderStorage); // index buffer
    m_per_renderable_ssbo_group.emplace(2 * size_of_v<Matrix4x4>, GPUBufferUsage::eShaderStorage); // transform

    m_per_material_params_ssbo_sp = VulkanBufferObject::makeShared();
    m_per_material_params_ssbo_sp->setUsage(GPUBufferUsage::eShaderStorage)
        .setSize(size_of_v<vk::DeviceAddress>)
        .create(m_context_p);
    

    m_indirect_call_buffer.setUsage(GPUBufferUsage::eIndirect)
        .create(m_context_p, size_of_v<vk::DrawIndirectCommand> + 1 * size_of_v<vk::DrawIndirectCommand>); // uint32_t(for IndirectDrawCount) + padding | vk::DrawIndirectCommand ...

    auto per_view_descriptor_set_layout_sp = VulkanDescriptorSetLayout::makeShared();
    per_view_descriptor_set_layout_sp->setBindings(vkconstants::per_view_bindings)
        .setIndex(to_integral(DescriptorSetBindingPoints::ePerView))
        .create(m_context_p);
    auto per_renderable_descriptor_set_layout_sp = VulkanDescriptorSetLayout::makeShared();
    per_renderable_descriptor_set_layout_sp->setBindings(vkconstants::per_renderable_bindings)
        .setIndex(to_integral(DescriptorSetBindingPoints::ePerRenderable))
        .create(m_context_p);
    m_per_view_descriptor_set.create(per_view_descriptor_set_layout_sp);
    m_per_renderable_descriptor_set.create(per_renderable_descriptor_set_layout_sp);

    vk::DescriptorBufferInfo per_view_buffer_info;
    per_view_buffer_info.setBuffer(m_per_view_uniform_buffer.getHandle())
        .setOffset(0)
        .setRange(m_per_view_uniform_buffer.getSizeInBytes());
    auto ds_updater = m_per_view_descriptor_set.generateUpdater();
    ds_updater.add(to_integral(PerViewBindingPoints::eCamera), per_view_buffer_info)
        .update();

    ds_updater = m_per_renderable_descriptor_set.generateUpdater();
    ds_updater.add(to_integral(PerRenderableBindingPoints::eVertexBuffer), m_per_renderable_ssbo_group[0].generateBufferInfo())
        .add(to_integral(PerRenderableBindingPoints::eIndexBuffer), m_per_renderable_ssbo_group[1].generateBufferInfo())
        .add(to_integral(PerRenderableBindingPoints::eTransform), m_per_renderable_ssbo_group[2].generateBufferInfo())
        .update();

    auto mesh_sp = Mesh::makeShared();
    mesh_sp->create(std::size(data::cube_positions));
    mesh_sp->setVertexData(VertexSemanticFlags::ePosition, as_bytes(data::cube_positions))
        .setVertexData(VertexSemanticFlags::eNormal, as_bytes(data::cube_normals))
        .setVertexData(VertexSemanticFlags::eTexCoord0, as_bytes(data::cube_uvs))
        .setIndexData(data::cube_indices);

    auto material_sp = Material::makeShared();
    auto image1_sp = Image::makeShared();
    image1_sp->loadFromFile("assets/images/bk.jpg", Image::Format::eRGBA8Uint);
    auto image2_sp = Image::makeShared();
    image2_sp->loadFromFile("assets/images/qt256.png", Image::Format::eRGBA8Uint);
    material_sp->addImage(image1_sp)
        .addImage(image2_sp);

    Model cube_model = ModelLoader().load("./assets/models/dinosaur/source/Rampaging T-Rex.glb");
    cube_model.addMaterial(material_sp);

    struct PBRMaterialParams
    {
        uint32_t m_base_color_texture_id = 0;
        uint32_t m_roughness_texture_id = 1;
        uint32_t m_metallic_texture_id = 2;
        uint32_t m_reflectance_texture_id = 3;
        uint32_t m_ambient_occlusion_texture_id = 4;
        uint32_t m_clearcoat_texture_id = 5;
        uint32_t m_clearcoat_roughness_texture_id = 6;
        uint32_t m_anisotropy_texture_id = 7;
        Vector4D<float> m_base_color = {1.0f, 1.0f, 1.0f, 1.0f};
        Vector4D<float> m_emissive_color = {1.0f, 1.0f, 1.0f, 1.0f};
    };
    PBRMaterialParams material_params; 
    m_material_params.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, size_of_v<PBRMaterialParams>);
    m_material_params.addWriteSegment({as_bytes_from_value(material_params)});
    vkutils::immediate_submit(m_context_p, vk::QueueFlagBits::eTransfer, [this, &cube_model](VulkanCommandBufferObject & cmd) {
        m_mesh.create(m_context_p, cmd, cube_model.getMesh(0));
        m_material_params.commit(cmd);
    });

    VulkanImage::SharedPointer cube_map_sp;
    VulkanImage::SharedPointer texture1_sp;
    VulkanImage::SharedPointer texture2_sp;
    VulkanSampler::SharedPointer sampler_sp;

    const Image & image = material_sp->getImage(0);
    texture1_sp = VulkanImage::makeShared();
    texture1_sp->setFormat(vk::Format::eR8G8B8A8Unorm)
        .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
        .setMipmapped(true)
        .setExtent({ image.getWidth(), image.getHeight(), 1u })
        .create(m_context_p);

    const Image & image2 = material_sp->getImage(1);
    texture2_sp = VulkanImage::makeShared();
    texture2_sp->setFormat(vk::Format::eR8G8B8A8Unorm)
        .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
        .setMipmapped(true)
        .setExtent({ image2.getWidth(), image2.getHeight(), 1u })
        .create(m_context_p);

    uint32_t cube_width = 1024;
    cube_map_sp = VulkanImage::makeShared();
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
    vk::SamplerCreateInfo sphere_sampler_info;
    sphere_sampler_info.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    auto sphere_sampler = device.createSamplerUnique(sphere_sampler_info);

    vkutils::immediate_submit(m_context_p, vk::QueueFlagBits::eGraphics, [&](VulkanCommandBufferObject & cmd) {
        texture1_sp->setData(cmd, image.getInterleavedDataSpan());
        texture1_sp->generateMipmaps(cmd);

        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(*texture1_sp->getLayout())
            .setImageView(texture1_sp->getDefaultView())
            .setSampler(sphere_sampler.get());

        const auto & layout_sp = stc_pipeline.getDescriptorSetLayoutSharedPtr(0);
        auto descriptor_set_sp = VulkanDescriptorSet::makeShared();
        descriptor_set_sp->create(layout_sp);
        auto descriptor_set_updater = descriptor_set_sp->generateUpdater();
        descriptor_set_updater.add(0, image_info).update();

        auto [w, h, z] = cube_map_sp->getExtent();
        VulkanFramebufferObjectCreateInfo fbo_info;
        fbo_info.setMaxExtent({w, h});
        VulkanFramebufferObject fbo;
        fbo.addColorAttachment(cube_map_sp)
            .create(m_context_p, fbo_info);
        cmd.acquireResource(descriptor_set_sp);
        cmd.bindPipeline(stc_pipeline.getType(), stc_pipeline.getHandle());
        cmd.bindDescriptorSets(stc_pipeline.getType(), stc_pipeline.getPipelineLayout(), descriptor_set_sp->getIndex(), descriptor_set_sp->getHandle(), nullptr);
        fbo.setViewportAndScissor(cmd);
        fbo.beginRendering(cmd);
        cmd.draw(36, 1, 0, 0); // draw with const data in shader program
        fbo.endRendering(cmd);
        cube_map_sp->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

        texture2_sp->setData(cmd, image2.getInterleavedDataSpan());
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
    skybox_ds_sp->create(m_skybox_pipeline.getDescriptorSetLayoutSharedPtr(to_integral(DescriptorSetBindingPoints::ePerMaterial)));
    m_skybox_material.create(skybox_ds_sp);
    m_skybox_material.setTexture(1, 0, cube_map_sp)
        .setSampler(1, 0, m_context_p->getSamplerManager().getShared(SamplerPreset::eEnvironmentMap))
        .commitUpdate();

    auto layout_sp = m_graphics_pipeline.getDescriptorSetLayoutSharedPtr(to_integral(DescriptorSetBindingPoints::ePerMaterial));
    auto material_ds_sp = VulkanDescriptorSet::makeShared();
    material_ds_sp->create(layout_sp);
    m_material.create(material_ds_sp);
    m_material.setTexture(2, 1, texture1_sp)
        .setTexture(2, 0, texture2_sp)
        .setSampler(1, 0, m_context_p->getSamplerManager().getShared(SamplerPreset::eColorMap))
        .setSampler(1, 1, m_context_p->getSamplerManager().getShared(SamplerPreset::eColorMap))
        .setParamsSSBO(0, m_per_material_params_ssbo_sp)
        .commitUpdate();
}

void lcf::VulkanRenderer::render(const Entity & camera, const Entity & render_target)
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
    projection.perspective(60.0f, static_cast<float>(width) / height, 0.1f, 1000.0f);
    auto &camera_transform = camera.getComponent<Transform>();
    const auto & camera_view = camera.getComponent<TransformInvertedWorldMatrix>();
    projection_view = projection * camera_view.getMatrix();
    m_per_view_uniform_buffer.addWriteSegment({as_bytes_from_value(projection), 0u})
        .addWriteSegment({as_bytes_from_value(camera_view.getMatrix()), sizeof(Matrix4x4)})
        .addWriteSegment({as_bytes_from_value(projection_view), 2 * sizeof(Matrix4x4)});
    
    static uint64_t frame_count = 0;
    ++frame_count;
    std::vector<Matrix4x4> model_matrices(2);
    model_matrices[0].scale(0.5f);
    float angle = 0.1f * frame_count;
    model_matrices[1].scale(0.5f);
    model_matrices[1].translateLocal({ 6.0f, 0.2f, 0.2f });
    model_matrices[1].rotateAroundSelf(Quaternion::fromAxisAndAngle({1.0f, 1.0f, 0.0f}, angle));

    auto & per_renderable_vertex_buffer_ssbo = m_per_renderable_ssbo_group[0];
    auto & per_renderable_index_buffer_ssbo = m_per_renderable_ssbo_group[1];
    auto & per_renderable_transform_ssbo = m_per_renderable_ssbo_group[2];

    per_renderable_vertex_buffer_ssbo.addWriteSegment({as_bytes_from_value(m_mesh.getVertexBufferAddress()), 0u});
    per_renderable_index_buffer_ssbo.addWriteSegment({as_bytes_from_value(m_mesh.getIndexBufferAddress()), 0u});
    per_renderable_transform_ssbo.addWriteSegment({as_bytes(model_matrices), 0u});

    uint32_t instance_count = 0;
    std::vector<vk::DrawIndirectCommand> indirect_calls(1);
    for (int i = 0; i < indirect_calls.size(); ++i) {
        /**
         * @brief 
         * firstVertex: draw index, use as geometry index to locate vertex buffer and index buffer
         * firstInstance: instance index, use as offset to locate model matrix, one for each instance
         */
        auto & indirect_call = indirect_calls[i];
        indirect_call.setVertexCount(m_mesh.getIndexCount())
            .setInstanceCount(2)
            .setFirstVertex(i)
            .setFirstInstance(instance_count);
        instance_count += indirect_call.instanceCount;
    }

    auto & data_transfer_cmd = current_frame_resources.data_transfer_command_buffer;

    uint32_t indirect_call_count = static_cast<uint32_t>(indirect_calls.size());
    m_indirect_call_buffer.addWriteSegment({as_bytes_from_value(indirect_call_count), 0})
        .addWriteSegment({as_bytes(indirect_calls), size_of_v<vk::DrawIndirectCommand>});

    m_per_material_params_ssbo_sp->addWriteSegment({as_bytes_from_value(m_material_params.getDeviceAddress()), 0u});

    data_transfer_cmd.begin({}); //- no need to wait
    m_indirect_call_buffer.commit(data_transfer_cmd);
    m_per_view_uniform_buffer.commit(data_transfer_cmd);
    m_per_renderable_ssbo_group.commitAll(data_transfer_cmd);
    m_per_material_params_ssbo_sp->commitWriteSegments(data_transfer_cmd);
    data_transfer_cmd.end();
    auto data_transfer_complete_info =  data_transfer_cmd.submit();

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
        .addWaitSubmitInfo(data_transfer_complete_info)
        .addSignalSubmitInfo(render_target_resources.getPresentReadySemaphore());
    cmd.submit();

    render_target_sp->finishRender();
    ++m_current_frame_index %= m_frame_resources.size();
}
