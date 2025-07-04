#pragma once

#include "VulkanRenderer.h"
#include "VulkanShaderProgram.h"
#include "vulkan_utililtie.h"
#include "ScreenManager.h"
#include "Vector.h"
#include "InterleavedBuffer.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "Image/Image.h"

lcf::VulkanRenderer::VulkanRenderer(VulkanContext *context) :
    Renderer(),
    m_context(context)
{
}

lcf::VulkanRenderer::~VulkanRenderer()
{
    auto device = m_context->getDevice();
    device.waitIdle();
}

void lcf::VulkanRenderer::setRenderTarget(render::RenderTarget * target)
{
    if (not target) { return; }
    target->create(); //make sure the target is created
    m_render_target = static_cast<VulkanSwapchain *>(target);
}

void lcf::VulkanRenderer::create()
{
    m_frame_resources.resize(3); //todo remove constant
    auto device = m_context->getDevice();
    vk::FenceCreateInfo fence_info;
    fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
    vk::SemaphoreCreateInfo semaphore_info;
    vk::CommandBufferAllocateInfo command_buffer_info;
    command_buffer_info.setCommandPool(m_context->getCommandPool())
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
    auto memory_allocator = m_context->getMemoryAllocator();
    vk::ImageCreateInfo image_info;
    auto current_screen = ScreenManager::getInstance()->getPrimaryScreen();
    auto [width, height] = current_screen->size() * current_screen->devicePixelRatio(); // todo add setResolution method to Renderer, call it before Renderer is created;

    for (auto &resources : m_frame_resources) {
        resources.frame_ready = device.createFenceUnique(fence_info);
        resources.target_available = device.createSemaphoreUnique(semaphore_info);
        resources.render_finished = device.createSemaphoreUnique(semaphore_info);
        resources.command_buffer = device.allocateCommandBuffers(command_buffer_info).front();
        resources.descriptor_manager.create(m_context);

        resources.framebuffer = VulkanFramebuffer::makeUnique(m_context);
        /*
        todo 直接使用    vk::PipelineRenderingCreateInfo rendering_info;填充framebuffer的创建信息
        */
        resources.framebuffer->setExtent({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
            .setColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat)
            .setDepthStencilAttachmentFormat(vk::Format::eD32Sfloat)
            .create();
    }
    // ! temporary
    VulkanShaderProgram::SharedPointer compute_shader_program = std::make_shared<VulkanShaderProgram>(m_context);
    compute_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Compute, "assets/shaders/gradient.comp");
    compute_shader_program->link();
    m_compute_pipeline = VulkanPipeline::makeUnique(m_context, compute_shader_program);
    m_compute_pipeline->create();

    VulkanShaderProgram::SharedPointer shader_program = std::make_shared<VulkanShaderProgram>(m_context);
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Vertex, "assets/shaders/vertex_buffer_test.vert");
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::Fragment, "assets/shaders/vertex_buffer_test.frag");
    shader_program->link();
    m_graphics_pipeline = VulkanPipeline::makeUnique(m_context, shader_program);
    m_graphics_pipeline->create();

    Vector3D positions[] = {
        {-0.5f, -0.5f, 0.0f},
        {0.5f, -0.5f, 0.0f},
        {0.5f,  0.5f, 0.0f},
        {-0.5f,  0.5f, 0.0f}
    };
    Vector3D colors[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    };
    Vector2D uvs[] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f}
    };
    struct Vertex
    {
        Vector3D position;
        Vector3D color;
        Vector2D uv;
    };  
    InterleavedBuffer vertex_data(4, { sizeof(Vector4D), sizeof(Vector4D), sizeof(Vector4D) });
    vertex_data.setData<Vector3D>(0, positions);
    vertex_data.setData<Vector3D>(1, colors);
    vertex_data.setData<Vector2D>(2, uvs);

    m_vertext_buffer = VulkanBuffer::makeUnique(m_context);
    m_vertext_buffer->setUsagePattern(GPUBuffer::UsagePattern::Static);
    m_vertext_buffer->setUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
    m_vertext_buffer->setData(vertex_data.getData(), vertex_data.getSizeInBytes());

    uint16_t index_data[] = { 0, 1, 2, 2, 3, 0 };
    m_index_buffer = VulkanBuffer::makeUnique(m_context);
    m_index_buffer->setUsagePattern(GPUBuffer::UsagePattern::Static);
    m_index_buffer->setUsageFlags(vk::BufferUsageFlagBits::eIndexBuffer);
    m_index_buffer->setData(index_data, sizeof(index_data));

    m_texture_image = VulkanImage::makeUnique(m_context);
    m_texture_image->setData(Image("assets/images/bk.jpg", 4));
    vk::SamplerCreateInfo sampler_info;
    sampler_info.setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest);
    m_texture_sampler = device.createSamplerUnique(sampler_info);
}

void lcf::VulkanRenderer::render()
{
    auto device = m_context->getDevice();
    auto &current_frame_resources = m_frame_resources[m_current_frame_index];
    bool &is_render_initiated = current_frame_resources.is_render_initiated;
    vk::Fence frame_ready = current_frame_resources.frame_ready.get();
    if (is_render_initiated) {
        if (device.waitForFences(frame_ready, true, UINT64_MAX) != vk::Result::eSuccess) {
            throw std::runtime_error("lcf::VulkanRenderer::render(): waitForFences failed");
        }
        device.resetFences(frame_ready);
        /*
            由于m_render_target->prepareForRender(&exchange_info);结果可能为false，本帧的渲染可能不会进行。
            如果渲染不进行，那么该信号量则不会被再次触发，但下一帧用的资源还是这一帧的资源。如果没有is_render_initiated变量，
            那么此处仍然会对frame_ready进行等待，进入死循环。
        */
    }
    vk::Semaphore target_available = current_frame_resources.target_available.get();
    vk::Semaphore render_finished = current_frame_resources.render_finished.get();
    VulkanRenderExchangeInfo exchange_info;
    exchange_info.setTargetAvailableSemaphore(target_available)
        .setRenderFinishedSemaphore(render_finished);
    is_render_initiated = m_render_target->prepareForRender(&exchange_info);
    if (not is_render_initiated) { return; }
    

    auto [draw_image, draw_image_view] = current_frame_resources.framebuffer->getColorAttachment(0);
    auto [depth_image, depth_image_view] = current_frame_resources.framebuffer->getDepthAttachment();
    vk::Image target_image = exchange_info.getTargetImage();
    vk::ImageView target_image_view = exchange_info.getTargetImageView();
    
    VulkanDescriptorManager &descriptor_manager = current_frame_resources.descriptor_manager;
    descriptor_manager.resetAllocatedSets();

    
    vk::CommandBuffer cmd_buffer = current_frame_resources.command_buffer; 
    cmd_buffer.reset();

    auto [width, height] = m_render_target->getSize();
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
  
    draw_image->transitLayout(cmd_buffer, vk::ImageLayout::eGeneral);

    auto descriptor_sets = *descriptor_manager.allocate(m_compute_pipeline->getDescriptorSetLayoutList());
    {
        vk::WriteDescriptorSet write_descriptor_set;
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(vk::ImageLayout::eGeneral)
            .setImageView(draw_image_view);
        const auto &descriptor_set_layout_table = m_compute_pipeline->getShaderProgram()->getDescriptorSetLayoutBindingTable();
        const auto &binding_info = descriptor_set_layout_table[0][0];
        write_descriptor_set.setDstSet(descriptor_sets[0])
            .setDstBinding(binding_info.binding)
            .setDescriptorType(binding_info.descriptorType)
            .setDescriptorCount(binding_info.descriptorCount);
        write_descriptor_set.setImageInfo(image_info);
        device.updateDescriptorSets(write_descriptor_set, nullptr);
    }

    cmd_buffer.bindPipeline(m_compute_pipeline->getType(), m_compute_pipeline->getHandle());
    cmd_buffer.bindDescriptorSets(m_compute_pipeline->getType(), m_compute_pipeline->getPipelineLayout(), 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
    cmd_buffer.dispatch(std::ceil(width / 16.0f), std::ceil(height / 16.0f), 1);

    draw_image->transitLayout(cmd_buffer, vk::ImageLayout::eColorAttachmentOptimal);
    depth_image->transitLayout(cmd_buffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::RenderingAttachmentInfo color_attachment_info;
    color_attachment_info.setImageView(draw_image_view)
        .setImageLayout(draw_image->getLayout())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue({{ 0.1f, 0.1f, 0.1f, 1.0f }});
    vk::RenderingAttachmentInfo depth_attachment_info;
    depth_attachment_info.setImageView(depth_image_view)
        .setImageLayout(depth_image->getLayout())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue({{ 1.0f, 0u }});
    vk::RenderingInfo rendering_info;
    rendering_info.setColorAttachments(color_attachment_info)
        .setPDepthAttachment(&depth_attachment_info)
        .setLayerCount(1)
        .setRenderArea({ { 0, 0 }, { width, height } });

    descriptor_sets = *descriptor_manager.allocate(m_graphics_pipeline->getDescriptorSetLayoutList());
    {
        vk::WriteDescriptorSet write_descriptor_set;
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(m_texture_image->getLayout())
            .setImageView(m_texture_image->getDefaultView())
            .setSampler(m_texture_sampler.get());
        const auto &descriptor_set_layout_table = m_graphics_pipeline->getShaderProgram()->getDescriptorSetLayoutBindingTable();
        const auto &binding_info = descriptor_set_layout_table[1][0];
        write_descriptor_set.setDstSet(descriptor_sets[1])
            .setDstBinding(binding_info.binding)
            .setDescriptorType(binding_info.descriptorType)
            .setDescriptorCount(binding_info.descriptorCount);
        write_descriptor_set.setImageInfo(image_info);
        device.updateDescriptorSets(write_descriptor_set, nullptr);
    }
    
    
    cmd_buffer.beginRendering(rendering_info);
    cmd_buffer.setViewport(0, viewport);
    cmd_buffer.setScissor(0, scissor);
    cmd_buffer.bindPipeline(m_graphics_pipeline->getType(), m_graphics_pipeline->getHandle());
    cmd_buffer.bindIndexBuffer(m_index_buffer->getHandle(), 0, vk::IndexType::eUint16);
    cmd_buffer.bindDescriptorSets(m_graphics_pipeline->getType(), m_graphics_pipeline->getPipelineLayout(), 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
    Matrix4x4 model; 
    model.scale(0.5f);
    vk::DeviceAddress vertex_buffer_address = m_vertext_buffer->getDeviceAddress();
    auto shader_program = m_graphics_pipeline->getShaderProgram();
    shader_program->setPushConstantData(vk::ShaderStageFlagBits::eVertex, {model.constData(), &vertex_buffer_address});
    shader_program->bindPushConstants(cmd_buffer);
    cmd_buffer.drawIndexed(6, 1, 0, 0, 0);

    model.translateLocal({ 0.2f, 0.2f, 0.2f });
    model.rotateAroundSelf(Quaternion::fromAxisAndAngle({ 0.0f, 0.0f, 1.0f }, 45.0f));
    shader_program->bindPushConstants(cmd_buffer);
    cmd_buffer.drawIndexed(6, 1, 0, 0, 0);


    cmd_buffer.endRendering();


    draw_image->transitLayout(cmd_buffer, vk::ImageLayout::eTransferSrcOptimal);

    // resolve to target
    vkutils::ImageLayoutTransitionAssistant layout_transition_assistant;
    layout_transition_assistant.setImage(target_image);
    layout_transition_assistant.transitTo(cmd_buffer, vk::ImageLayout::eTransferDstOptimal);
    vkutils::CopyAssistant copy_assistant(cmd_buffer);
    copy_assistant.copy(draw_image->getHandle(), target_image, {width, height}, {width, height});
    layout_transition_assistant.transitTo(cmd_buffer, vk::ImageLayout::ePresentSrcKHR);
    cmd_buffer.end();

    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info;
    submit_info.setWaitSemaphores(target_available)
       .setWaitDstStageMask(wait_stage)
       .setCommandBuffers(cmd_buffer)
        .setSignalSemaphores(render_finished);
    m_context->getQueue(vk::QueueFlagBits::eGraphics).submit(submit_info, frame_ready);

    m_render_target->finishRender();
    ++m_current_frame_index %= m_frame_resources.size();
}
