#pragma once

#include "VulkanRenderer.h"
#include "VulkanShaderProgram.h"
#include "vulkan_utililtie.h"
#include "Vector.h"
#include "InterleavedBuffer.h"
#include "Matrix.h"
#include "constants.h"
#include "Quaternion.h"
#include "Image/Image.h"
#include "VulkanDescriptorWriter.h"
#include "Entity.h"
#include "Transform.h"
#include <boost/align.hpp>
#include <boost/container/small_vector.hpp>

lcf::VulkanRenderer::VulkanRenderer(VulkanContext *context) :
    m_context_p(context)
{
    m_camera_entity.requireComponent<Transform>();
}

lcf::VulkanRenderer::~VulkanRenderer()
{
    auto device = m_context_p->getDevice();
    device.waitIdle();
}

void lcf::VulkanRenderer::setRenderTarget(const RenderTarget::SharedPointer & render_target)
{
    if (not render_target) { return; }
    render_target->create(); //make sure the target is created
    m_render_target = std::static_pointer_cast<VulkanSwapchain>(render_target);
}

void lcf::VulkanRenderer::setCamera(const Entity &camera_entity)
{
    if (camera_entity.hasComponent<Transform>()) {
        m_camera_entity = camera_entity;
    }
}

void lcf::VulkanRenderer::create()
{
    m_frame_resources.resize(3); //todo remove constant
    auto device = m_context_p->getDevice();
    vk::FenceCreateInfo fence_info;
    fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
    vk::SemaphoreCreateInfo semaphore_info;
    vk::CommandBufferAllocateInfo command_buffer_info;
    command_buffer_info.setCommandPool(m_context_p->getCommandPool())
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
    auto memory_allocator = m_context_p->getMemoryAllocator();
    vk::ImageCreateInfo image_info;

    auto render_target = m_render_target.lock();
    auto [width, height] = render_target->getMaximalExtent();

    VulkanShaderProgram::SharedPointer compute_shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    compute_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Compute, "assets/shaders/gradient.comp");
    compute_shader_program->link();

    ComputePipelineCreateInfo compute_pipeline_info(compute_shader_program);
    m_compute_pipeline.create(m_context_p, compute_pipeline_info);

    VulkanShaderProgram::SharedPointer shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Vertex, "assets/shaders/vertex_buffer_test.vert");
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Fragment, "assets/shaders/vertex_buffer_test.frag");
    shader_program->link();
    GraphicPipelineCreateInfo graphic_pipeline_info;
    graphic_pipeline_info.setShaderProgram(shader_program)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
        .setRasterizationSamples(vk::SampleCountFlagBits::e4)
        .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat);
    m_graphics_pipeline.create(m_context_p, graphic_pipeline_info);

    for (auto &resources : m_frame_resources) {
        resources.render_finished = device.createSemaphoreUnique(semaphore_info);
        resources.command_buffer.create(m_context_p);
        resources.descriptor_manager.create(m_context_p);

        VulkanFramebufferObjectCreateInfo fbo_info;
        fbo_info.setMaxExtent({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
            .addColorFormats(graphic_pipeline_info.getColorAttachmentFormats())
            .setSampleCount(graphic_pipeline_info.getRasterizationSamples())
            .setDepthStencilFormat(graphic_pipeline_info.getDepthAttachmentFormat())
            .setResolveMode(vk::ResolveModeFlagBits::eAverage);
        resources.fbo.create(m_context_p, fbo_info);
    }
    // ! temporary

    m_global_uniform_buffer = VulkanBufferObject::makeUnique();
    m_global_uniform_buffer->setUsage(GPUBufferUsage::eUniform)
        // .setSize(sizeof(Matrix4x4))
        .create(m_context_p);

    InterleavedBuffer vertex_data;
    vertex_data.addField<std_alignment_traits<Vector3D>>()
        .addField<std_alignment_traits<Vector3D>>()
        .addField<std_alignment_traits<Vector2D>>()
        .create(std::size(constants::cube_indices));
    vertex_data.setData(0, std::span(constants::cube_positions));
    vertex_data.setData(1, std::span(constants::cube_colors));
    vertex_data.setData(2, std::span(constants::cube_uvs));

    m_vertex_buffer.setUsage(GPUBufferUsage::eVertex).create(m_context_p);
    m_vertex_buffer.addWriteSegment({vertex_data.getDataSpan()}).commitWriteSegments();
    
    m_index_buffer.setUsage(GPUBufferUsage::eIndex).create(m_context_p);
    m_index_buffer.addWriteSegment({std::span(constants::cube_indices)}).commitWriteSegments();

    m_texture_image = VulkanImage::makeUnique();
    m_texture_image->create(m_context_p, Image("assets/images/bk.jpg", 4));
    vk::SamplerCreateInfo sampler_info;
    sampler_info.setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest);
    m_texture_sampler = device.createSamplerUnique(sampler_info);
}

void lcf::VulkanRenderer::render()
{
    auto device = m_context_p->getDevice();
    auto &current_frame_resources = m_frame_resources[m_current_frame_index];
    if (m_render_target.expired()) { return; }
    
    const auto &render_target = m_render_target.lock();
    if (not render_target->prepareForRender()) { return; }

    
    VulkanCommandBufferObject & cmd_buffer = current_frame_resources.command_buffer; 
    cmd_buffer.prepareForRecording();

    auto [width, height] = render_target->getExtent();
    auto clear_value = vk::ClearValue{ { 0.0f, 1.0f, 0.0f, 1.0f } };
    vk::Viewport viewport;
    viewport.setX(0.0f).setY(height)
        .setWidth(static_cast<float>(width))
        .setHeight(-static_cast<float>(height))
        .setMinDepth(0.0f).setMaxDepth(1.0f);
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 }).setExtent({ width, height });
    vk::CommandBufferBeginInfo cmd_begin_info;
    cmd_buffer.begin(cmd_begin_info);
    /*
    ! DescriptorSet和Buffer都是command buffer的Resource，需要允许cmd.acquire(resource)
    ! 由cmd控制资源生命周期
    */
    VulkanDescriptorManager &descriptor_manager = current_frame_resources.descriptor_manager;
    descriptor_manager.resetAllocatedSets();


    auto descriptor_sets = *descriptor_manager.allocate(m_compute_pipeline.getDescriptorSetLayoutList());

    auto &current_framebuffer = current_frame_resources.fbo;
    Matrix4x4 projection, projection_view;
    projection.perspective(90.0f, static_cast<float>(width) / height, 0.1f, 100.0f);

    auto &camera_transform = m_camera_entity.getComponent<Transform>();
    projection_view = projection * camera_transform.getInvertedLocalMatrix();
    if (m_current_frame_index % 3 == 1) {
        projection_view.setToIdentity();
    }
    m_global_uniform_buffer->addWriteSegment({std::span(&projection_view, 1), 0u});
    m_global_uniform_buffer->commitWriteSegments();

    descriptor_sets = *descriptor_manager.allocate(m_graphics_pipeline.getDescriptorSetLayoutList());
    {
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(m_texture_image->getLayout())
            .setImageView(m_texture_image->getDefaultView())
            .setSampler(m_texture_sampler.get());
        vk::DescriptorBufferInfo buffer_info;
        
        VulkanDescriptorWriter writer(m_context_p, m_graphics_pipeline.getShaderProgram()->getDescriptorSetLayoutBindingTable(), descriptor_sets) ;
        writer.add(1, 0, image_info)
            .write();

        vk::DescriptorBufferInfo test_buffer_info;
        test_buffer_info.setBuffer(m_global_uniform_buffer->getHandle())
            .setOffset(0)
            .setRange(m_global_uniform_buffer->getSize());
        vk::WriteDescriptorSet write_descriptor_set;
        write_descriptor_set.setDstSet(descriptor_sets[0])
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
            .setDescriptorCount(1)
            .setBufferInfo(test_buffer_info);
        device.updateDescriptorSets(write_descriptor_set, nullptr);
    }
    
    cmd_buffer.setViewport(0, viewport);
    cmd_buffer.setScissor(0, scissor);
    cmd_buffer.bindPipeline(m_graphics_pipeline.getType(), m_graphics_pipeline.getHandle());

    current_framebuffer.beginRendering(&cmd_buffer);
 
    Matrix4x4 model; 
    model.scale(0.5f);

    auto shader_program = m_graphics_pipeline.getShaderProgram();
    vk::DeviceSize offset = 0;
    cmd_buffer.bindVertexBuffers(0, m_vertex_buffer.getHandle(), offset);
    cmd_buffer.bindIndexBuffer(m_index_buffer.getHandle(), 0, vk::IndexType::eUint16);


    uint32_t dynamic_offset = 0;
    cmd_buffer.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), 0, descriptor_sets[0], dynamic_offset);

    cmd_buffer.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), 1, descriptor_sets[1], nullptr);

    shader_program->setPushConstantData(vk::ShaderStageFlagBits::eVertex, {model.getConstData()});
    shader_program->bindPushConstants(cmd_buffer);
    cmd_buffer.drawIndexed(36, 1, 0, 0, 0);
    static float angle = 0.0f;
    model.translateLocal({ 1.0f, 0.2f, 0.2f });
    model.rotateAroundSelf(Quaternion::fromAxisAndAngle(Vector3D(1.0f, 1.0f, 0.0f), angle));
    angle += 0.1f;
    shader_program->bindPushConstants(cmd_buffer);
    cmd_buffer.drawIndexed(36, 1, 0, 0, 0);

    current_framebuffer.endRendering(&cmd_buffer);
    auto & target_image_sp = render_target->getTargetImageSharedPointer();
    // blit to target
    auto msaa_attachment = current_framebuffer.getMSAAResolveAttachment();
    if (msaa_attachment) {
        VulkanAttachment target_attachment(target_image_sp);
        msaa_attachment->blitTo(&cmd_buffer, target_attachment, vk::Filter::eLinear,
            {{0, 0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height), 1}},
            {{0, 0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height), 1}});
    }

    target_image_sp->transitLayout(&cmd_buffer, vk::ImageLayout::ePresentSrcKHR);
    cmd_buffer.end();

    vk::Semaphore render_finished = current_frame_resources.render_finished.get();
    vk::SemaphoreSubmitInfo wait_info;
    wait_info.setSemaphore(render_target->getTargetAvailableSemaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    cmd_buffer.addWaitSubmitInfo(wait_info)
        .addSignalSubmitInfo(render_finished);
    cmd_buffer.submit(vk::QueueFlagBits::eGraphics);

    render_target->finishRender(render_finished);
    ++m_current_frame_index %= m_frame_resources.size();
}
