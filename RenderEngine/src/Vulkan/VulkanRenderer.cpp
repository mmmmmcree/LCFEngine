#pragma once

#include "VulkanRenderer.h"
#include "VulkanShaderProgram.h"
#include "vulkan_utililtie.h"
#include "Vector.h"
#include "InterleavedBuffer.h"
#include "Matrix.h"
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

    for (auto &resources : m_frame_resources) {
        resources.render_finished = device.createSemaphoreUnique(semaphore_info);
        resources.command_buffer.create(m_context_p);
        resources.descriptor_manager.create(m_context_p);

        resources.framebuffer = VulkanFramebuffer::makeUnique(m_context_p);
        /*
        todo 直接使用    vk::PipelineRenderingCreateInfo rendering_info;填充framebuffer的创建信息
        */
        resources.framebuffer->setExtent({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
            .setColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat)
            .setSampleCount(vk::SampleCountFlagBits::e4)
            .setDepthStencilAttachmentFormat(vk::Format::eD32Sfloat)
            .create();
    }
    // ! temporary


    m_global_uniform_buffer = VulkanTimelineBuffer::makeUnique(m_context_p);
    m_global_uniform_buffer->setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
        | vk::BufferUsageFlagBits::eTransferDst);
    m_global_uniform_buffer->setSize(sizeof(Matrix4x4));
    m_global_uniform_buffer->create();

    VulkanShaderProgram::SharedPointer compute_shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    compute_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Compute, "assets/shaders/gradient.comp");
    compute_shader_program->link();
    m_compute_pipeline = VulkanPipeline::makeUnique(m_context_p, compute_shader_program);
    m_compute_pipeline->create();

    VulkanShaderProgram::SharedPointer shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Vertex, "assets/shaders/vertex_buffer_test.vert");
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Fragment, "assets/shaders/vertex_buffer_test.frag");
    shader_program->link();
    m_graphics_pipeline = VulkanPipeline::makeUnique(m_context_p, shader_program);
    m_graphics_pipeline->create();

    // Vector3D positions[] = {
    //     {-0.5f, -0.5f, 0.0f},
    //     {0.5f, -0.5f, 0.0f},
    //     {0.5f,  0.5f, 0.0f},
    //     {-0.5f,  0.5f, 0.0f}
    // };
    // Vector3D colors[] = {
    //     {1.0f, 0.0f, 0.0f},
    //     {0.0f, 1.0f, 0.0f},
    //     {0.0f, 0.0f, 1.0f},
    //     {1.0f, 1.0f, 1.0f}
    // };
    // Vector2D uvs[] = {
    //     {0.0f, 1.0f},
    //     {1.0f, 1.0f},
    //     {1.0f, 0.0f},
    //     {0.0f, 0.0f}
    // };
    // uint16_t index_data[] = { 0, 1, 2, 2, 3, 0 };
    Vector3D cube_positions[] = {
        {-1.0f, -1.0f, -1.0f}, {-1.0f, +1.0f, -1.0f}, {+1.0f, +1.0f, -1.0f}, {+1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, -1.0f},
        {+1.0f, -1.0f, -1.0f}, {+1.0f, -1.0f, +1.0f}, {+1.0f, +1.0f, +1.0f}, {+1.0f, +1.0f, -1.0f},
        {+1.0f, +1.0f, -1.0f}, {+1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, -1.0f},
        {-1.0f, +1.0f, -1.0f}, {-1.0f, +1.0f, +1.0f}, {-1.0f, -1.0f, +1.0f}, {-1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, +1.0f}, {+1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, +1.0f},
    };
    Vector3D cube_colors[] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
    };
    Vector2D cube_uvs[] = {
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
    };
    uint16_t index_data[] = {
        2, 0, 1, 2, 3, 0,
        4, 6, 5, 4, 7, 6,
        8, 10 ,9, 8, 11, 10,
        12, 14, 13, 12, 15, 14,
        16, 18,17, 16, 19, 18,
        20, 21, 22, 20, 22, 23
    };
    InterleavedBuffer vertex_data;
    vertex_data.addField<std_alignment_traits<Vector3D>>()
        .addField<std_alignment_traits<Vector3D>>()
        .addField<std_alignment_traits<Vector2D>>()
        .create(std::size(index_data));
    vertex_data.setData(0, std::span(cube_positions));
    vertex_data.setData(1, std::span(cube_colors));
    vertex_data.setData(2, std::span(cube_uvs));

    m_vertext_buffer = VulkanBuffer::makeUnique(m_context_p);
    m_vertext_buffer->setUsagePattern(GPUBuffer::UsagePattern::eStatic);
    m_vertext_buffer->setUsageFlags(vk::BufferUsageFlagBits::eVertexBuffer);
    m_vertext_buffer->setData(vertex_data.getData(), vertex_data.getSizeInBytes());

    m_index_buffer = VulkanBuffer::makeUnique(m_context_p);
    m_index_buffer->setUsagePattern(GPUBuffer::UsagePattern::eStatic);
    m_index_buffer->setUsageFlags(vk::BufferUsageFlagBits::eIndexBuffer);
    m_index_buffer->setData(index_data, sizeof(index_data));

    m_texture_image = VulkanImage::makeUnique(m_context_p);
    m_texture_image->setData(Image("assets/images/bk.jpg", 4));
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

    vk::Image target_image = render_target->getTargetImage();
    
    VulkanCommandBuffer & cmd_buffer = current_frame_resources.command_buffer; 

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

    // cmd_buffer.setDescriptorBufferOffsetsEXT()

    auto descriptor_sets = *descriptor_manager.allocate(m_compute_pipeline->getDescriptorSetLayoutList());

    auto &current_framebuffer = current_frame_resources.framebuffer;
    Matrix4x4 projection, projection_view;
    projection.perspective(90.0f, static_cast<float>(width) / height, 0.1f, 100.0f);

    auto &camera_transform = m_camera_entity.getComponent<Transform>();
    projection_view = projection * camera_transform.getInvertedLocalMatrix();
    // m_global_uniform_buffer->acquireNextBuffer();
    if (m_current_frame_index % 3 == 1) {
        projection_view.setToIdentity();
    }

    descriptor_sets = *descriptor_manager.allocate(m_graphics_pipeline->getDescriptorSetLayoutList());
    {
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(m_texture_image->getLayout())
            .setImageView(m_texture_image->getDefaultView())
            .setSampler(m_texture_sampler.get());
        vk::DescriptorBufferInfo buffer_info;
        

        VulkanDescriptorWriter writer(m_context_p, m_graphics_pipeline->getShaderProgram()->getDescriptorSetLayoutBindingTable(), descriptor_sets) ;
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
    
    m_global_uniform_buffer->beginWrite();
    m_global_uniform_buffer->setData(projection_view.getConstData(), sizeof(Matrix4x4));
    m_global_uniform_buffer->endWrite();

    cmd_buffer.setViewport(0, viewport);
    cmd_buffer.setScissor(0, scissor);
    cmd_buffer.bindPipeline(m_graphics_pipeline->getType(), m_graphics_pipeline->getHandle());

    current_framebuffer->beginRendering();
 
    Matrix4x4 model; 
    model.scale(0.5f);

    auto shader_program = m_graphics_pipeline->getShaderProgram();
    vk::DeviceSize offset = 0;
    cmd_buffer.bindVertexBuffers(0, m_vertext_buffer->getHandle(), offset);
    cmd_buffer.bindIndexBuffer(m_index_buffer->getHandle(), 0, vk::IndexType::eUint16);


    uint32_t dynamic_offset = 0;
    cmd_buffer.bindDescriptorSets(m_graphics_pipeline->getType(), m_graphics_pipeline->getPipelineLayout(), 0, descriptor_sets[0], dynamic_offset);

    cmd_buffer.bindDescriptorSets(m_graphics_pipeline->getType(), m_graphics_pipeline->getPipelineLayout(), 1, descriptor_sets[1], nullptr);

    shader_program->setPushConstantData(vk::ShaderStageFlagBits::eVertex, {model.getConstData()});
    shader_program->bindPushConstants(cmd_buffer);
    cmd_buffer.drawIndexed(36, 1, 0, 0, 0);
    static float angle = 0.0f;
    model.translateLocal({ 1.0f, 0.2f, 0.2f });
    model.rotateAroundSelf(Quaternion::fromAxisAndAngle(Vector3D(1.0f, 1.0f, 0.0f), angle));
    angle += 0.1f;
    shader_program->bindPushConstants(cmd_buffer);
    cmd_buffer.drawIndexed(36, 1, 0, 0, 0);

    current_framebuffer->endRendering();

    auto resolved_draw_image = current_framebuffer->getMSAAResolveAttachment().first;
    resolved_draw_image->transitLayout(vk::ImageLayout::eTransferSrcOptimal);

    // resolve to target
    vkutils::ImageLayoutTransitionAssistant layout_transition_assistant;
    layout_transition_assistant.setImage(target_image);
    layout_transition_assistant.transitTo(cmd_buffer, vk::ImageLayout::eTransferDstOptimal);
    vkutils::CopyAssistant copy_assistant(cmd_buffer);
    copy_assistant.copy(resolved_draw_image->getHandle(), target_image, {width, height}, {width, height});
    layout_transition_assistant.transitTo(cmd_buffer, render_target->getTargetImageLayout());
    cmd_buffer.end();


    vk::Semaphore render_finished = current_frame_resources.render_finished.get();
    vk::SemaphoreSubmitInfo wait_info;
    wait_info.setSemaphore(render_target->getTargetAvailableSemaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    cmd_buffer.addWaitSubmitInfo(wait_info)
        .addSignalSubmitInfo(render_finished)
        .addSignalSubmitInfo(m_global_uniform_buffer->generateSubmitInfo());
    cmd_buffer.submit(vk::QueueFlagBits::eGraphics);

    render_target->finishRender(render_finished);
    ++m_current_frame_index %= m_frame_resources.size();
}
