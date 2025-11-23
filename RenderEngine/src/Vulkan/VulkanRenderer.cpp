#include "VulkanRenderer.h"
#include "VulkanShaderProgram.h"
#include "vulkan_utililtie.h"
#include "Matrix.h"
#include "geometry_data.h"
#include "Quaternion.h"
#include "Image/Image.h"
#include "VulkanDescriptorWriter.h"
#include "Entity.h"
#include "Transform.h"
#include "VulkanDescriptorSetLayout.h"
#include <boost/container/small_vector.hpp>
#include "as_bytes.h"
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
        resources.command_buffer.create(m_context_p);
        resources.descriptor_manager.create(m_context_p);

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
        .setSize(sizeof(Matrix4x4) * 3) // projection, view, projection_view
        .create(m_context_p);
    
    m_per_renderable_transform_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setSize(2 * size_of_v<Matrix4x4>)
        .create(m_context_p);

    m_per_renderable_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setSize(size_of_v<vk::DeviceAddress>)
        .create(m_context_p);

    m_per_renderable_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setSize(size_of_v<vk::DeviceAddress>)
        .create(m_context_p);

    m_indirect_call_buffer.setUsage(GPUBufferUsage::eIndirect)
        .setSize(size_of_v<vk::DrawIndirectCommand> + 1 * size_of_v<vk::DrawIndexedIndirectCommand>) // uint32_t(for IndirectDrawCount) + padding | vk::DrawIndirectCommand ... 
        .create(m_context_p);


    auto & global_descriptor_manager = m_context_p->getDescriptorManager();
    vk::DescriptorSetLayoutCreateInfo per_view_descriptor_set_layout_info;
    per_view_descriptor_set_layout_info.setBindings(vkconstants::per_view_bindings);
    auto per_view_descriptor_set_layout = device.createDescriptorSetLayoutUnique(per_view_descriptor_set_layout_info);
    vk::DescriptorSetLayoutCreateInfo per_renderable_descriptor_set_layout_info;
    per_renderable_descriptor_set_layout_info.setBindings(vkconstants::per_renderable_bindings);
    auto per_renderable_descriptor_set_layout = device.createDescriptorSetLayoutUnique(per_renderable_descriptor_set_layout_info);
    m_per_view_descriptor_set = global_descriptor_manager.allocate(per_view_descriptor_set_layout.get());
    m_per_renderable_descriptor_set = global_descriptor_manager.allocate(per_renderable_descriptor_set_layout.get());

    vk::DescriptorBufferInfo per_view_buffer_info;
    per_view_buffer_info.setBuffer(m_per_view_uniform_buffer.getHandle())
        .setOffset(0)
        .setRange(m_per_view_uniform_buffer.getSize());
    VulkanDescriptorWriter per_view_writer(m_context_p, vkconstants::per_view_bindings);
    per_view_writer.add(to_integral(PerViewBindingPoints::eCamera), per_view_buffer_info)
        .write(m_per_view_descriptor_set);

    vk::DescriptorBufferInfo per_renderable_vertex_buffer_info(m_per_renderable_vertex_buffer.getHandle(), 0, vk::WholeSize);
    vk::DescriptorBufferInfo per_renderable_index_buffer_info(m_per_renderable_index_buffer.getHandle(), 0, vk::WholeSize);
    vk::DescriptorBufferInfo per_renderable_transform_buffer_info(m_per_renderable_transform_buffer.getHandle(), 0, vk::WholeSize);
    VulkanDescriptorWriter per_renderable_writer(m_context_p, vkconstants::per_renderable_bindings);
    per_renderable_writer.add(to_integral(PerRenderableBindingPoints::eVertexBuffer), per_renderable_vertex_buffer_info)
        .add(to_integral(PerRenderableBindingPoints::eIndexBuffer), per_renderable_index_buffer_info)
        .add(to_integral(PerRenderableBindingPoints::eTransform), per_renderable_transform_buffer_info)
        .write(m_per_renderable_descriptor_set);

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
    // material_sp->addImage(Image::makeShared("assets/images/bk.jpg", 4))
    //     .addImage(Image::makeShared("assets/images/qt256.png", 4));
    material_sp->addImage(image1_sp)
        .addImage(image2_sp);

    Model cube_model = ModelLoader().load("./assets/models/dinosaur/source/Rampaging T-Rex.glb");
    cube_model.addMaterial(material_sp);
    // Model cube_model;
    // cube_model.addMesh(mesh_sp) 
    //     .addMaterial(material_sp);

    vkutils::immediate_submit(m_context_p, [this, &cube_model](VulkanCommandBufferObject & cmd) {
        m_mesh.create(m_context_p, cmd, cube_model.getMesh(0));
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

    vkutils::immediate_submit(m_context_p, [&](VulkanCommandBufferObject & cmd) {
        texture1_sp->setData(cmd, image.getInterleavedDataSpan());
        texture1_sp->generateMipmaps(cmd);
        const auto & ds_prototype = stc_pipeline.getDescriptorSetPrototype(0);
        auto & global_descriptor_manager = m_context_p->getDescriptorManager();
        auto ds = global_descriptor_manager.allocate(ds_prototype.getLayout());
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(*texture1_sp->getLayout())
            .setImageView(texture1_sp->getDefaultView())
            .setSampler(sphere_sampler.get());
        VulkanDescriptorWriter writer = ds_prototype.generateWriter();
        writer.add(0, image_info).write(ds);
        auto [w, h, z] = cube_map_sp->getExtent();
        VulkanFramebufferObjectCreateInfo fbo_info;
        fbo_info.setMaxExtent({w, h});
        VulkanFramebufferObject fbo;
        fbo.addColorAttachment(cube_map_sp)
            .create(m_context_p, fbo_info);
        cmd.bindPipeline(stc_pipeline.getType(), stc_pipeline.getHandle());
        cmd.bindDescriptorSets(stc_pipeline.getType(), stc_pipeline.getPipelineLayout(), ds_prototype.getSet(), ds, nullptr);
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

    vk::SamplerCreateInfo sampler_info;
    sampler_info.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
        .setMinLod(0.0f)
        .setMaxLod(static_cast<float>(cube_map_sp->getMipLevelCount()))
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMaxAnisotropy(4.0f)
        .setAnisotropyEnable(true);
    sampler_sp = VulkanSampler::makeShared();
    sampler_sp->create(m_context_p, sampler_info);

    m_material.create(m_context_p, m_graphics_pipeline.getShaderProgram()->getDescriptorSetLayoutBindingList(to_integral(DescriptorSetBindingPoints::ePerMaterial)));
    m_material.setTexture(1, 1, texture1_sp)
        .setTexture(1, 0, texture2_sp)
        .setSampler(2, 0, sampler_sp)
        .commitUpdate();

    m_skybox_material.create(m_context_p, skybox_pipeline_info.getShaderProgram()->getDescriptorSetLayoutBindingList(to_integral(DescriptorSetBindingPoints::ePerMaterial)));
    m_skybox_material.setTexture(1, 0, cube_map_sp)
        .setSampler(1, 0, sampler_sp)
        .commitUpdate();
}

void lcf::VulkanRenderer::render(const Entity & camera, const Entity & render_target)
{
    auto device = m_context_p->getDevice();
    auto &current_frame_resources = m_frame_resources[m_current_frame_index];

    VulkanCommandBufferObject & cmd = current_frame_resources.command_buffer; 
    cmd.prepareForRecording();

    auto render_target_wp = render_target.getComponent<VulkanSwapchain::WeakPointer>();
    if (render_target_wp.expired()) { return; }
    const auto & render_target_sp = render_target_wp.lock();
    if (not render_target_sp->prepareForRender()) { return; }

    auto [width, height] = render_target_sp->getExtent();
    vk::Viewport viewport;
    viewport.setX(0.0f).setY(height)
        .setWidth(static_cast<float>(width))
        .setHeight(-static_cast<float>(height))
        .setMinDepth(0.0f).setMaxDepth(1.0f);
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 }).setExtent({ width, height });
    
    cmd.begin(vk::CommandBufferBeginInfo{});
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
    
    auto &descriptor_manager = current_frame_resources.descriptor_manager;
    descriptor_manager.resetAllocatedSets();
    
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    std::vector<Matrix4x4> model_matrices(2);
    model_matrices[0].scale(0.5f);
    static float angle = 0.0f;
    angle += 0.1f;
    model_matrices[1].scale(0.5f);
    model_matrices[1].translateLocal({ 6.0f, 0.2f, 0.2f });
    model_matrices[1].rotateAroundSelf(Quaternion::fromAxisAndAngle({1.0f, 1.0f, 0.0f}, angle));

    m_per_renderable_vertex_buffer.addWriteSegment({as_bytes_from_value(m_mesh.getVertexBufferAddress()), 0u});
    m_per_renderable_index_buffer.addWriteSegment({as_bytes_from_value(m_mesh.getIndexBufferAddress()), 0u});
    m_per_renderable_transform_buffer.addWriteSegment({as_bytes(model_matrices), 0u});

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
    uint32_t indirect_call_count = 1;
    m_indirect_call_buffer.addWriteSegment({as_bytes_from_value(indirect_call_count), 0})
        .addWriteSegment({as_bytes(indirect_calls), size_of_v<vk::DrawIndirectCommand>})
        .commitWriteSegments(cmd);

    // delay commit
    m_per_view_uniform_buffer.commitWriteSegments(cmd);
    m_per_renderable_vertex_buffer.commitWriteSegments(cmd);
    m_per_renderable_index_buffer.commitWriteSegments(cmd);
    m_per_renderable_transform_buffer.commitWriteSegments(cmd);

    current_framebuffer.beginRendering(cmd);

    uint32_t dynamic_offset = 0;
    cmd.bindPipeline(m_graphics_pipeline.getType(), m_graphics_pipeline.getHandle());
    cmd.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), to_integral(DescriptorSetBindingPoints::ePerView), m_per_view_descriptor_set, dynamic_offset);
    cmd.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), to_integral(DescriptorSetBindingPoints::ePerRenderable), m_per_renderable_descriptor_set, nullptr);
    cmd.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), to_integral(DescriptorSetBindingPoints::ePerMaterial), m_material.getDescriptorSet(), nullptr);
    
    cmd.drawIndirectCount(m_indirect_call_buffer.getHandle(), sizeof(vk::DrawIndirectCommand),
        m_indirect_call_buffer.getHandle(), 0,
        1, sizeof(vk::DrawIndirectCommand));
    
    cmd.bindPipeline(m_skybox_pipeline.getType(), m_skybox_pipeline.getHandle());

    cmd.bindDescriptorSets(m_skybox_pipeline.getType(), m_skybox_pipeline.getPipelineLayout(), to_integral(DescriptorSetBindingPoints::ePerView), m_per_view_descriptor_set, dynamic_offset);
    cmd.bindDescriptorSets(m_skybox_pipeline.getType(), m_skybox_pipeline.getPipelineLayout(), to_integral(DescriptorSetBindingPoints::ePerMaterial), m_skybox_material.getDescriptorSet(), nullptr);
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
        .addSignalSubmitInfo(render_target_resources.getPresentReadySemaphore());
    cmd.submit(vk::QueueFlagBits::eGraphics);

    render_target_sp->finishRender();
    ++m_current_frame_index %= m_frame_resources.size();
}
