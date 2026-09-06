#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/DeviceContext.h"  
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/Image.h"
#include "vk_core/descriptor_set/info_structs.h"
// descriptor set pool
#include "vk_core/descriptor_set/pool/info_structs.h"
#include "vk_core/descriptor_set/pool/DescriptorSetLayout.h"
#include "vk_core/descriptor_set/pool/DescriptorSetAllocator.h"
#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"
// descriptor set buffer
#include "vk_core/descriptor_set/buffer/entry.h"
#include "vk_core/descriptor_set/buffer/DescriptorSetLayout.h"
#include "vk_core/descriptor_set/buffer/DescriptorSetAllocator.h"
#include "vk_core/descriptor_set/buffer/DescriptorSetProxy.h"

#include "vk_core/sampler/info_structs.h"
#include "vk_core/sampler/Sampler.h"
#include "vk_core/error.h"
#include "vk_core/pipeline/graphics/entry.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/pipeline/graphics/GraphicsPipeline.h"
#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/DynamicGraphicsState.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/shader/entry.h"
#include "vk_core/command/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/queue/Queue.h"
#include "common/window_handle.h"
#include "win/Window.h"
#include "shader_core/config.h"
#include "shader_core/ShaderCompiler.h"
#include "image/Image.h"
#include <atomic>
#include <thread>
#include <variant>
#include <array>
#include <optional>
#include "log.h"
#include "enums/enum_cast.h"
#include "vkc_config.h"

using namespace lcf;
namespace stdv = std::views;

constexpr const char * k_window_title = "hello descriptor set - descriptor buffer";

namespace lcf {
template <>
struct enum_mapping_traits<ShaderTypeFlagBits, vk::ShaderStageFlagBits>
{
    static constexpr std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits> mappings[] = {
        { ShaderTypeFlagBits::eVertex, vk::ShaderStageFlagBits::eVertex },
        { ShaderTypeFlagBits::eFragment, vk::ShaderStageFlagBits::eFragment },
    };
};
} // namespace lcf

int main()
{
    std::filesystem::path shader_assets_dir = SHADER_ASSETS_DIR;
    lcf::sc::Config::instance()
        .registerVirtualPath("shaders", shader_assets_dir)
        .setDefaultGlslEntryPoint("main");
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::entry::register_context(inst_ext_manifest, device_ext_manifest);
    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks.setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::entry::register_debug_utils(inst_ext_manifest, vkc::dbg::SeverityFlags::eError | vkc::dbg::SeverityFlags::eWarning | vkc::dbg::SeverityFlags::eVerbose, debug_callbacks);
    vkc::probe::CapabilityRegistry capabilities {inst_ext_manifest, device_ext_manifest};
    vkc::probe::register_capabilities(capabilities);
    vkc::entry::register_dynamic_render(device_ext_manifest);
    vkc::entry::register_descriptor_buffer(device_ext_manifest);
    //- in this example, we use shader constants to draw a triangle, so we should enable shaderDrawParameters feature
    device_ext_manifest.addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan13Features::synchronization2>)
        .addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters>);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);

    vkc::InstanceContextCreateInfo instance_info;
    instance_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);

    vkc::InstanceContext instance_context;
    if (auto ec = instance_context.create(instance_info)) {
        lcf_log_error("Failed to create instance_context: {}", ec.message());
        return 1;
    }
    win::WindowCreateInfo window_info;
    window_info.setTitle(k_window_title);
    win::Window window;
    if (auto ec = window.create(window_info)) {
        lcf_log_error("Failed to create window: {}", ec.message());
        return 1;
    }
    vkc::wsi::WindowHandle wsi_window_handle = vkce::to_wsi_window_handle(window.handle());
    auto expected_surface = vkc::wsi::create_surface(instance_context.getInstance(), wsi_window_handle);
    if (not expected_surface) {
        lcf_log_error("Failed to create surface: {}", expected_surface.error().message());
        return 1;
    }
    auto & surface = expected_surface.value();

    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    vkc::probe::configure_physical_device(physical_device_select_info);
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest);
    vkc::DeviceContextCreateInfo device_context_info;
    device_context_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPhysicalDeviceSelectInfo(physical_device_select_info);
    vkc::QueueRequest graphics_queue_request {
        vk::QueueFlagBits::eGraphics,
        {},
        vkc::QueueSubmissionThreadTag {0},
        1.0f
    };
    vkc::QueueRequest present_queue_request {
        vk::QueueFlagBits::eGraphics,
        {},
        vkc::QueueSubmissionThreadTag {1},
        1.0f,
        surface.get()
    };
    vkc::QueueKey graphics_queue_key = device_context_info.addQueueRequest(graphics_queue_request);
    vkc::QueueKey present_queue_key = device_context_info.addQueueRequest(present_queue_request);

    vkc::DeviceContext device_context;
    if (auto ec = device_context.create(instance_context.getInstance(), device_context_info)) {
        lcf_log_error("Failed to create render_device_context: {}", ec.message());
        return 1;
    }
    const vkc::MemoryAllocator & memory_allocator = device_context.getMemoryAllocator();
    vk::Device device = device_context.getDevice();

    vkc::Queue gfx_queue;
    if (auto ec = gfx_queue.create(device_context.getLogicalQueue(graphics_queue_key))) {
        lcf_log_error("Failed to create graphics queue: {}", ec.message());
        return 1;
    }

    lcf::img::Image texture_data;
    auto expected_texture_file_info = lcf::img::read_image_info(std::filesystem::path(IMAGE_ASSETS_DIR) / "vulkanlogo.png");
    if (not expected_texture_file_info) {
        lcf_log_error("Failed to read texture info: {}", expected_texture_file_info.error().message());
        return 1;
    }
    if (auto ec = texture_data.loadFromFileGpuFriendly(*expected_texture_file_info)) {
        lcf_log_error("Failed to load texture: {}", ec.message());
        return 1;
    }
    vk::BufferCreateInfo staging_buffer_info;
    staging_buffer_info.setSize(texture_data.getDataSpan().size_bytes())
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);
    vkc::MemoryAllocationInfo staging_allocation_info;
    staging_allocation_info.setAccess(vkc::MemoryAccess::eHostSequentialWrite);
    vkc::Buffer staging_buffer;
    if (auto ec = staging_buffer.create(memory_allocator, staging_buffer_info, staging_allocation_info)) {
        lcf_log_error("Failed to create texture staging buffer: {}", ec.message());
        return 1;
    }
    if (auto ec = staging_buffer.copyFromMemory(texture_data.getDataSpan())) {
        lcf_log_error("Failed to copy texture data: {}", ec.message());
        return 1;
    }
    if (auto ec = staging_buffer.flush()) {
        lcf_log_error("Failed to flush texture data: {}", ec.message());
        return 1;
    }

    vk::ImageCreateInfo texture_image_info;
    texture_image_info.setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Srgb)
        .setExtent({texture_data.getWidth(), texture_data.getHeight(), 1u})
        .setMipLevels(1u)
        .setArrayLayers(1u)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    vkc::MemoryAllocationInfo texture_allocation_info;
    texture_allocation_info.setAccess(vkc::MemoryAccess::eDeviceLocal);
    vkc::Image texture_image;
    if (auto ec = texture_image.create(memory_allocator, texture_image_info, texture_allocation_info)) {
        lcf_log_error("Failed to create texture image: {}", ec.message());
        return 1;
    }
    vk::ImageSubresourceRange texture_subresource_range {vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};
    auto expected_texture_view = texture_image.createView(texture_subresource_range, vk::ImageViewType::e2D);
    if (not expected_texture_view) {
        lcf_log_error("Failed to create texture view: {}", expected_texture_view.error().message());
        return 1;
    }
    vkc::ImageView texture_view = std::move(*expected_texture_view);

    vkc::SamplerInfo sampler_info;
    sampler_info.setMinMagFilter(vk::Filter::eLinear, vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setAddressMode(vk::SamplerAddressMode::eClampToEdge);
    vkc::Sampler sampler;
    if (auto ec = sampler.create(device, sampler_info)) {
        lcf_log_error("Failed to create sampler: {}", ec.message());
        return 1;
    }

    vkc::DescriptorSetLayoutInfo descriptor_set_layout_info;
    descriptor_set_layout_info.addBindingInfo(vk::DescriptorType::eSampledImage, 1u, vk::ShaderStageFlagBits::eFragment)
        .addBindingInfo(vk::DescriptorType::eSampler, 1u, vk::ShaderStageFlagBits::eFragment);
    // descriptor set pool
    vkc::dsp::DescriptorSetLayout dsp_descriptor_set_layout;
    if (auto ec = dsp_descriptor_set_layout.create(device, descriptor_set_layout_info)) {
        lcf_log_error("Failed to create descriptor set layout: {}", ec.message());
        return 1;
    }
    vkc::dsp::DescriptorSetAllocatorInfo dsp_descriptor_allocator_info;
    std::array descriptor_pool_sizes {
        vk::DescriptorPoolSize {vk::DescriptorType::eSampledImage, 8u},
        vk::DescriptorPoolSize {vk::DescriptorType::eSampler, 8u}
    };
    dsp_descriptor_allocator_info.setPoolSizes(descriptor_pool_sizes).setMaxSetsPerPool(8u);
    vkc::dsp::DescriptorSetAllocator dsp_descriptor_allocator;
    if (auto ec = dsp_descriptor_allocator.create(device, dsp_descriptor_allocator_info)) {
        lcf_log_error("Failed to create descriptor set allocator: {}", ec.message());
        return 1;
    }
    vkc::dsp::DescriptorSetProxy dsp_descriptor_set;
    if (auto ec = dsp_descriptor_set.create(dsp_descriptor_allocator, dsp_descriptor_set_layout)) {
        lcf_log_error("Failed to create descriptor set proxy: {}", ec.message());
        return 1;
    }
    dsp_descriptor_set.setImage(0u, texture_view, vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSampler(1u, sampler);
    // descriptor set buffer
    vkc::dsb::DescriptorSetLayout dsb_descriptor_set_layout;
    if (auto ec = dsb_descriptor_set_layout.create(device, descriptor_set_layout_info)) {
        lcf_log_error("Failed to create descriptor set layout: {}", ec.message());
        return 1;
    }
    vkc::dsb::DescriptorSetAllocator dsb_descriptor_allocator;
    if (auto ec = dsb_descriptor_allocator.create(memory_allocator)) {
        lcf_log_error("Failed to create descriptor set allocator: {}", ec.message());
        return 1;
    }
    vkc::dsb::DescriptorSetProxy dsb_descriptor_set;
    if (auto ec = dsb_descriptor_set.create(dsb_descriptor_allocator, dsb_descriptor_set_layout)) {
        lcf_log_error("Failed to create descriptor set proxy: {}", ec.message());
        return 1;
    }
    dsb_descriptor_set.setImage(0u, texture_view, vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSampler(1u, sampler);

    vkc::wsi::probed::Swapchain swapchain;
    if (auto ec = swapchain.create(
        std::move(surface),
        device_context.getPhysicalDevice(),
        device_context.getLogicalQueue(present_queue_key)))
    {
        lcf_log_error("Failed to create swapchain: {}", ec.message());
        return 1;
    }

    //- compile shader and create shader_program_info
    sc::ShaderCompiler shader_compiler;
    auto expected_compile_result = shader_compiler.compileSlangSourceToSpv("shaders://triangle.slang");
    if (not expected_compile_result) {
        lcf_log_error("Failed to compile shader: {}", expected_compile_result.error().message());
        return 1;
    }
    lcf_log_info("Shader compiled successfully.");
    auto & spv_units = expected_compile_result.value();

    vkc::ShaderProgramInfo shader_program_info;
    for (const auto & spv_unit : spv_units) {
        vk::ShaderStageFlagBits stage = enum_cast<vk::ShaderStageFlagBits>(spv_unit.getStage());

        vkc::ShaderStageInfo shader_stage_info;
        shader_stage_info.setStage(stage)
            .setCode(spv_unit.getCode())
            .setEntryPoint(spv_unit.getEntryPoint());
        shader_program_info.addStageInfo(std::move(shader_stage_info));
    }
    // shader_program_info.addDescriptorSetLayout(0u, dsp_descriptor_set_layout.handle()); // dsp
    shader_program_info.addDescriptorSetLayout(0u, dsb_descriptor_set_layout.handle()); // dsb

    //- declare the attachment set: one color attachment, no resolve, no depth stencil
    vkc::AttachmentSetInfoBuilder attachment_set_builder;
    vkc::ColorAttachmentKey color_key = attachment_set_builder.addColorAttachment();
    vkc::AttachmentSetInfo attachment_set = attachment_set_builder.build();
    //- create render targets
    auto [width, height] = window.getPixelSize();
    vkc::RenderTargetInfo render_target_info {attachment_set};
    render_target_info.setExtent({width, height})
        .setFormat(color_key, vk::Format::eR8G8B8A8Unorm);
    std::array<vkc::Image, 2> render_target_images;
    std::array<vkc::RenderTarget, 2> render_targets;
    for (auto & image : render_target_images) {
        vk::ImageCreateInfo image_info;
        image_info.setImageType(vk::ImageType::e2D)
            .setFormat(attachment_set.at(color_key).getFormat())
            .setExtent({width, height, 1u})
            .setMipLevels(1u)
            .setArrayLayers(1u)
            .setSamples(render_target_info.getSampleCount())
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
            .setInitialLayout(vk::ImageLayout::eUndefined);
        vkc::MemoryAllocationInfo mem_alloc_info;
        mem_alloc_info.setAccess(vkc::MemoryAccess::eDeviceLocal);
        if (auto ec = image.create(memory_allocator, image_info, mem_alloc_info)) {
            lcf_log_error("Failed to create image: {}", ec.message());
            return 1;
        }
    }
    for (auto & render_target : render_targets) {
        if (auto ec = render_target.build(render_target_info)) {
            lcf_log_error("Failed to create render_target: {}", ec.message());
            return 1;
        }
    }
    for (auto && [render_target, image] : stdv::zip(render_targets, render_target_images)) {
        if (auto ec = render_target.setColorAttachment(color_key, image)) {
            lcf_log_error("Failed to set color attachment: {}", ec.message());
            return 1;
        }
    }

    vkc::ViewportStateInfo viewport_state_info;
    viewport_state_info.addViewport(0, 0, width, height)
        .addScissor(0, 0, width, height);
    vkc::ColorBlendStateInfo color_blend_state_info {attachment_set.getColorAttachmentCount()};
    color_blend_state_info.setColorBlendAttachmentState(
        0u,
        vk::BlendFactor::eSrcAlpha,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd);
    vkc::GraphicsPipelineInfo graphic_pipeline_info;
    graphic_pipeline_info.setShaderProgramInfo(shader_program_info)
        .addFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT) // dsb
        .setViewportStateInfo(viewport_state_info)
        .setColorBlendStateInfo(color_blend_state_info);

    vkc::DynamicRenderInfo dynamic_render_info {attachment_set};
    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitAttributes(color_key,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits2::eBlit,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageUsageFlagBits::eTransferSrc);
    vkc::DynamicRender dynamic_render;
    if (auto ec = dynamic_render.create(dynamic_render_info)) {
        lcf_log_error("Failed to create dynamic_render: {}", ec.message());
        return 1;
    }
    vkc::GraphicsPipeline dynamic_graphics_pipeline;
    if (auto ec = dynamic_graphics_pipeline.create(device, graphic_pipeline_info, dynamic_render.makeScopeInfo())) {
        lcf_log_error("Failed to create dynamic_graphics_pipeline: {}", ec.message());
        return 1;
    }

    vkc::CommandBufferAllocateInfo upload_cmd_alloc_info;
    upload_cmd_alloc_info.setLevel(vk::CommandBufferLevel::ePrimary).setCount(1u);
    auto expected_upload_batch = gfx_queue.allocateCommandBufferBatch(upload_cmd_alloc_info);
    if (not expected_upload_batch) {
        lcf_log_error("Failed to allocate texture upload command buffer: {}", expected_upload_batch.error().message());
        return 1;
    }
    auto & upload_batch = *expected_upload_batch;
    auto expected_upload_cmd = upload_batch.acquireProxy();
    if (not expected_upload_cmd) {
        lcf_log_error("Failed to acquire texture upload command buffer: {}", expected_upload_cmd.error().message());
        return 1;
    }
    auto & upload_cmd = *expected_upload_cmd;
    upload_cmd.begin(vk::CommandBufferBeginInfo {vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::ImageMemoryBarrier2 to_transfer_dst;
    to_transfer_dst.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setImage(texture_image.handle())
        .setSubresourceRange(texture_subresource_range);
    vk::DependencyInfo to_transfer_dependency;
    to_transfer_dependency.setImageMemoryBarriers(to_transfer_dst);
    upload_cmd.pipelineBarrier2(to_transfer_dependency);
    vk::BufferImageCopy copy_region;
    copy_region.setImageSubresource({vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u})
        .setImageExtent(texture_image_info.extent);
    upload_cmd.copyBufferToImage(staging_buffer.handle(), texture_image.handle(), vk::ImageLayout::eTransferDstOptimal, copy_region);
    vk::ImageMemoryBarrier2 to_shader_read;
    to_shader_read.setSrcStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead)
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImage(texture_image.handle())
        .setSubresourceRange(texture_subresource_range);
    vk::DependencyInfo to_shader_dependency;
    to_shader_dependency.setImageMemoryBarriers(to_shader_read);
    upload_cmd.pipelineBarrier2(to_shader_dependency);
    upload_cmd.pinLease(staging_buffer.lease()).pinLease(texture_image.lease());
    upload_cmd.end();
    upload_batch.collect(std::move(upload_cmd));
    auto expected_upload_submit = gfx_queue.submit(std::move(upload_batch));
    if (not expected_upload_submit) {
        lcf_log_error("Failed to submit texture upload: {}", expected_upload_submit.error().message());
        return 1;
    }

    //- render loop
    std::atomic<bool> running {true};
    std::thread render_thread([&] {
        static uint64_t frame = 0;
        std::array<vkc::SubmissionToken, 2> present_blit_finish_tokens;
        while (running.load(std::memory_order_relaxed)) {
            vkc::CommandBufferAllocateInfo cmd_alloc_info;
            cmd_alloc_info.setLevel(vk::CommandBufferLevel::ePrimary)
                .setCount(1);
            auto expected_cmd_buffer_batch = gfx_queue.allocateCommandBufferBatch(cmd_alloc_info);
            if (not expected_cmd_buffer_batch) {
                lcf_log_error("Failed to allocate command buffer: {}", expected_cmd_buffer_batch.error().message());
                continue;
            }
            vkc::CommandBufferBatch & cmd_buffer_batch = expected_cmd_buffer_batch.value();
            auto expected_cmd_buffer_proxy = cmd_buffer_batch.acquireProxy();
            if (not expected_cmd_buffer_proxy) {
                lcf_log_error("Failed to acquire proxy: {}", expected_cmd_buffer_proxy.error().message());
                continue;
            }
            vkc::CommandBufferProxy & cmd = expected_cmd_buffer_proxy.value();
            auto & render_target = render_targets[frame % 2];
            vk::CommandBufferBeginInfo cmd_begin_info {};

            cmd.begin(cmd_begin_info);

            // 当前 bind 同时承担 descriptor buffer 的脏数据更新和绑定：更新阶段可能录制
            // vkCmdPipelineBarrier2/vkCmdCopyBuffer，不能处于 dynamic rendering scope 内，
            // 因此先在 dynamic_render.begin() 外调用。TODO: 拆分为 updateIfDirty() 和 bind()，
            // 之后 updateIfDirty() 保持在 scope 外，纯 bind() 可以在 scope 内切换 descriptor set。
            dsb_descriptor_set.bind(cmd, vk::PipelineBindPoint::eGraphics, dynamic_graphics_pipeline.getPipelineLayout());
            dynamic_render.begin(cmd, render_target);
            // dsp_descriptor_set.bind(cmd, vk::PipelineBindPoint::eGraphics, dynamic_graphics_pipeline.getPipelineLayout());
            dynamic_graphics_pipeline.bind(cmd);
            cmd.draw(6, 1, 0, 0);
            dynamic_render.end(cmd);

            cmd.end();
            cmd.addWaitInfo(present_blit_finish_tokens[frame % 2]);
            cmd_buffer_batch.collect(std::move(cmd));
            auto expected_submit_result = gfx_queue.submit(std::move(cmd_buffer_batch));
            gfx_queue.collectGarbage();
            if (not expected_submit_result) { continue; }
            auto & submit_semaphore_info = expected_submit_result.value();
            std::array<vk::Offset3D, 2> src_offsets {{ {0, 0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height), 1} }};
            const vkc::Image & present_image = render_target.getAttachment(color_key).getImage();
            auto expected_present_result = swapchain.present(src_offsets, present_image, present_image.lease(), submit_semaphore_info);
            if (expected_present_result) {
                present_blit_finish_tokens[frame % 2] = expected_present_result.value();
                ++frame;
                continue;
            }
            auto ec = expected_present_result.error();
            if (ec == vkc::errc::surface_zero_size or ec == vkc::errc::present_skipped_for_resize) { continue; }
            lcf_log_error("present failed: {}", ec.message());
        }
    });

    window.setResizeCallback([&swapchain](const win::ResizeEvent &) {
        if (auto ec = swapchain.resizeToFit(); ec and ec != vkc::errc::surface_zero_size) {
            lcf_log_error("resizeToFit failed: {}", ec.message());
        }
    });
    if (auto ec = window.show()) {
        lcf_log_error("Failed to show window: {}", ec.message());
        return 1;
    }
    while (running.load(std::memory_order_relaxed)) {
        for (const win::WindowEvent & event : window.pollEvents()) {
            if (std::holds_alternative<win::CloseEvent>(event)) {
                running.store(false, std::memory_order_relaxed);
            }
        }
    }
    render_thread.join();
    device_context.getDevice().waitIdle();
    return 0;
}
