#pragma once

#include "VulkanRenderer.h"
#include "VulkanShaderProgram.h"
#include "vulkan_utililtie.h"
#include "Vector.h"
#include "InterleavedBuffer.h"
#include "Matrix.h"
#include "geometry_data.h"
#include "Quaternion.h"
#include "Image/Image.h"
#include "VulkanDescriptorWriter.h"
#include "Entity.h"
#include "Transform.h"
#include "VulkanDescriptorSetLayout.h"
#include <boost/container/small_vector.hpp>
#include "common/glsl_alignment_traits.h"

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
    auto memory_allocator = m_context_p->getMemoryAllocator();

    auto render_target = m_render_target.lock();
    auto [max_width, max_height] = render_target->getMaximalExtent();

    VulkanShaderProgram::SharedPointer compute_shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    compute_shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eCompute, "assets/shaders/gradient.comp");
    compute_shader_program->link();

    ComputePipelineCreateInfo compute_pipeline_info(compute_shader_program);
    m_compute_pipeline.create(m_context_p, compute_pipeline_info);

    VulkanShaderProgram::SharedPointer shader_program = std::make_shared<VulkanShaderProgram>(m_context_p);
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex, "assets/shaders/vertex_buffer_test.vert");
    shader_program->addShaderFromGlslFile(ShaderTypeFlagBits::eFragment, "assets/shaders/vertex_buffer_test.frag");
    shader_program->link();
    GraphicPipelineCreateInfo graphic_pipeline_info;
    graphic_pipeline_info.setShaderProgram(shader_program)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
        .setRasterizationSamples(vk::SampleCountFlagBits::e4)
        .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat);
    m_graphics_pipeline.create(m_context_p, graphic_pipeline_info);

    for (auto &resources : m_frame_resources) {
        resources.render_finished = device.createSemaphoreUnique({});
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
        .setSize(sizeof(Matrix4x4))
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
        .setSize(sizeof(vk::DrawIndirectCommand) + 1 * sizeof(vk::DrawIndexedIndirectCommand)) // uint32_t(for IndirectDrawCount) + padding | vk::DrawIndirectCommand ... 
        .create(m_context_p);

    InterleavedBuffer vertex_data_ssbo;
    vertex_data_ssbo.addField<alignment_traits<glsl::std140::Vector3D>>()
        .addField<alignment_traits<float>>()
        .addField<alignment_traits<glsl::std140::Vector3D>>()
        .addField<alignment_traits<float>>()
        .create(std::size(data::cube_positions));

    ConstStrideIterator<float> begin_u(data::cube_uvs, size_of_v<Vector2D>);
    ConstStrideIterator<float> begin_v(&data::cube_uvs[0].y, size_of_v<Vector2D>);
    vertex_data_ssbo.setData(0, std::span(data::cube_positions))
        .setData(1, std::ranges::subrange(begin_u, begin_u + std::size(data::cube_uvs)))
        .setData(2, std::span(data::cube_colors))
        .setData(3, std::ranges::subrange(begin_v, begin_v + std::size(data::cube_uvs)));

    m_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .setSize(vertex_data_ssbo.getSizeInBytes())
        .create(m_context_p);
    m_vertex_buffer.addWriteSegment({vertex_data_ssbo.getDataSpan()}).commitWriteSegments();
    
    m_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .setSize(sizeof(data::cube_indices))
        .create(m_context_p);
    m_index_buffer.addWriteSegment({std::span(data::cube_indices)}).commitWriteSegments();

    m_texture_image = VulkanImage::makeUnique();
    m_texture_image->create(m_context_p, Image("assets/images/bk.jpg", 4));
    vk::SamplerCreateInfo sampler_info;
    sampler_info.setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest);
    m_texture_sampler = device.createSamplerUnique(sampler_info);

    m_global_descriptor_manager.create(m_context_p);
    vk::DescriptorSetLayoutCreateInfo per_view_descriptor_set_layout_info;
    per_view_descriptor_set_layout_info.setBindings(vkconstants::per_view_bindings);
    auto per_view_descriptor_set_layout = device.createDescriptorSetLayoutUnique(per_view_descriptor_set_layout_info);
    vk::DescriptorSetLayoutCreateInfo per_renderable_descriptor_set_layout_info;
    per_renderable_descriptor_set_layout_info.setBindings(vkconstants::per_renderable_bindings);
    auto per_renderable_descriptor_set_layout = device.createDescriptorSetLayoutUnique(per_renderable_descriptor_set_layout_info);
    m_per_view_descriptor_set = *m_global_descriptor_manager.allocate(per_view_descriptor_set_layout.get());
    m_per_renderable_descriptor_set = *m_global_descriptor_manager.allocate(per_renderable_descriptor_set_layout.get());

    vk::DescriptorBufferInfo per_view_buffer_info;
    per_view_buffer_info.setBuffer(m_per_view_uniform_buffer.getHandle())
        .setOffset(0)
        .setRange(sizeof(Matrix4x4));
    VulkanDescriptorWriter per_view_writer(m_context_p, m_per_view_descriptor_set, vkconstants::per_view_bindings);
    per_view_writer.add(enum_cast(PerViewBindingPoints::eCamera), per_view_buffer_info)
        .write();

    vk::DescriptorBufferInfo per_renderable_vertex_buffer_info(m_per_renderable_vertex_buffer.getHandle(), 0, vk::WholeSize);
    vk::DescriptorBufferInfo per_renderable_index_buffer_info(m_per_renderable_index_buffer.getHandle(), 0, vk::WholeSize);
    vk::DescriptorBufferInfo per_renderable_transform_buffer_info(m_per_renderable_transform_buffer.getHandle(), 0, vk::WholeSize);
    VulkanDescriptorWriter per_renderable_writer(m_context_p, m_per_renderable_descriptor_set, vkconstants::per_renderable_bindings);
    per_renderable_writer.add(enum_cast(PerRenderableBindingPoints::eVertexBuffer), per_renderable_vertex_buffer_info)
        .add(enum_cast(PerRenderableBindingPoints::eIndexBuffer), per_renderable_index_buffer_info)
        .add(enum_cast(PerRenderableBindingPoints::eTransform), per_renderable_transform_buffer_info)
        .write();
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
    vk::Viewport viewport;
    viewport.setX(0.0f).setY(height)
        .setWidth(static_cast<float>(width))
        .setHeight(-static_cast<float>(height))
        .setMinDepth(0.0f).setMaxDepth(1.0f);
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 }).setExtent({ width, height });

    cmd_buffer.begin(vk::CommandBufferBeginInfo{});
    /*
    ! DescriptorSet和Buffer都是command buffer的Resource，需要允许cmd.acquire(resource)
    ! 由cmd控制资源生命周期
    */

    auto &current_framebuffer = current_frame_resources.fbo;
    Matrix4x4 projection, projection_view;
    projection.perspective(90.0f, static_cast<float>(width) / height, 0.1f, 100.0f);

    auto &camera_transform = m_camera_entity.getComponent<Transform>();
    projection_view = projection * camera_transform.getInvertedLocalMatrix();
    if (m_current_frame_index % 3 == 1) {
        projection_view.setToIdentity();
    }
    m_per_view_uniform_buffer.addWriteSegment({std::span(&projection_view, 1), 0u});
    
    VulkanDescriptorManager &descriptor_manager = current_frame_resources.descriptor_manager;
    descriptor_manager.resetAllocatedSets();
    auto per_material_descriptor_set = *descriptor_manager.allocate(m_graphics_pipeline.getDescriptorSetLayout(enum_cast(DescriptorSetBindingPoints::ePerMaterial)));
    {
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(m_texture_image->getLayout())
            .setImageView(m_texture_image->getDefaultView())
            .setSampler(m_texture_sampler.get());

        auto per_material_bindings = m_graphics_pipeline.getDescriptorSetLayoutBindings(enum_cast(DescriptorSetBindingPoints::ePerMaterial));
        VulkanDescriptorWriter writer(m_context_p, per_material_descriptor_set, per_material_bindings);
        writer.add(1, image_info)
            .write();
    }
    
    cmd_buffer.setViewport(0, viewport);
    cmd_buffer.setScissor(0, scissor);
    cmd_buffer.bindPipeline(m_graphics_pipeline.getType(), m_graphics_pipeline.getHandle());

    std::vector<Matrix4x4> model_matrices(2);
    model_matrices[0].scale(0.5f);
    static float angle = 0.0f;
    angle += 0.1f;
    model_matrices[1].scale(0.5f);
    model_matrices[1].translateLocal({ 1.0f, 0.2f, 0.2f });
    model_matrices[1].rotateAroundSelf(Quaternion::fromAxisAndAngle(Vector3D(1.0f, 1.0f, 0.0f), angle));

    m_per_renderable_vertex_buffer.addWriteSegment({std::span(&m_vertex_buffer.getDeviceAddress(), 1), 0u});
    m_per_renderable_index_buffer.addWriteSegment({std::span(&m_index_buffer.getDeviceAddress(), 1), 0u});
    m_per_renderable_transform_buffer.addWriteSegment({std::span(model_matrices), 0u});

    // cmd_buffer.drawIndirectCount();

    uint32_t instance_count = 0;
    std::vector<vk::DrawIndirectCommand> indirect_calls(1);
    for (int i = 0; i < indirect_calls.size(); ++i) {
        /**
         * @brief 
         * firstVertex: draw index, use as geometry index to locate vertex buffer and index buffer
         * firstInstance: instance index, use as offset to locate model matrix, one for each instance
         */
        auto & indirect_call = indirect_calls[i];
        indirect_call.setVertexCount(std::size(data::cube_indices))
            .setInstanceCount(2)
            .setFirstVertex(i)
            .setFirstInstance(instance_count);
        instance_count += indirect_call.instanceCount;
    }
    uint32_t indirect_call_count = 1;
    m_indirect_call_buffer.addWriteSegment({std::span(&indirect_call_count, 1), 0})
        .addWriteSegment({std::span(indirect_calls), size_of_v<vk::DrawIndirectCommand>})
        .commitWriteSegments();

    // delay commit
    m_per_view_uniform_buffer.commitWriteSegments();

    m_per_renderable_vertex_buffer.commitWriteSegments();
    m_per_renderable_index_buffer.commitWriteSegments();
    m_per_renderable_transform_buffer.commitWriteSegments();
    current_framebuffer.beginRendering(&cmd_buffer);

    uint32_t dynamic_offset = 0;
    cmd_buffer.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), 0, m_per_view_descriptor_set, dynamic_offset);

    cmd_buffer.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), 2, per_material_descriptor_set, nullptr);

    cmd_buffer.bindDescriptorSets(m_graphics_pipeline.getType(), m_graphics_pipeline.getPipelineLayout(), 1, m_per_renderable_descriptor_set, nullptr);

    // uint32_t instance_count = 0;
    // for (int i = 0; i < 1; ++i) {
    //     /**
    //      * @brief 
    //      * firstVertex: draw index, use as geometry index to locate vertex buffer and index buffer
    //      * firstInstance: instance index, use as offset to locate model matrix, one for each instance
    //      */
    //     cmd_buffer.draw(std::size(data::cube_indices), 2, i, instance_count);
    //     instance_count += 2;
    // }
    //todo add a structural size for VulkanBufferObject, like InterleavedBuffer
    // cmd_buffer.drawIndirect(m_indirect_call_buffer.getHandle(), 0, indirect_calls.size(), size_of_v<vk::DrawIndirectCommand>);
    cmd_buffer.drawIndirectCount(m_indirect_call_buffer.getHandle(), sizeof(vk::DrawIndirectCommand),
        m_indirect_call_buffer.getHandle(), 0,
        1, sizeof(vk::DrawIndirectCommand));

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
