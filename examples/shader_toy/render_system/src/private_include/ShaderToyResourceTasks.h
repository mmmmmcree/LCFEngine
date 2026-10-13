#pragma once

#include "RenderTaskSystem.h"
#include "ShaderToyInstance.h"
#include "image/Image.h"
#include "shader_core/spirv.h"
#include <filesystem>
#include <memory>
#include <string>

namespace lcf::vkc {

class DeviceContext;
class Queue;

}

namespace lcf::shader_toy {

struct ShaderToyTaskContext
{
    using QueueFamilyIndices = std::array<uint32_t, 2>;

    RenderTaskSystem & m_task_system;
    const vkc::DeviceContext & m_device_context;
    vkc::Queue & m_gfx_task_queue;
    vkc::Queue & m_graphics_queue;
    vkc::dsp::DescriptorSetAllocator & m_descriptor_allocator;
    QueueFamilyIndices m_queue_family_indices;
};

struct ShaderSource
{
    std::string m_code;
    std::string m_module_name;
    std::filesystem::path m_directory;
};

using ImageResourceInfoList = ShaderToyInstanceInfo::ImageResourceInfoList;
using InstanceInfoPointer = std::shared_ptr<ShaderToyInstanceInfo>;

std::expected<img::Image, std::error_code> load_image(const std::filesystem::path & path) noexcept;
std::expected<ShaderToyImageResourceInfo, std::error_code> upload_image(
    const ShaderToyTaskContext & context, const img::Image & pixels) noexcept;
asio::awaitable<std::expected<ShaderToyImageResourceInfo, std::error_code>> prepare_sampled_image(
    const ShaderToyTaskContext & context, const ShaderToyImageResourceInfo & image_resource_info);
std::expected<ShaderSource, std::error_code> load_shader_source(const std::filesystem::path & path) noexcept;
std::expected<sc::spirv::UnitList, std::error_code> compile_shader_source(const ShaderSource & source) noexcept;
vkc::ShaderProgramInfo create_shader_program_info(const sc::spirv::UnitList & units);
std::error_code create_buffer_resources(const ShaderToyTaskContext & context,
    const vkc::ImageDescription & image_description, uint32_t count, ImageResourceInfoList & image_resource_infos) noexcept;
std::error_code initialize_buffer_resources(vkc::Queue & queue, ImageResourceInfoList & image_resource_infos) noexcept;
std::error_code prepare_pass(const ShaderToyTaskContext & context,
    std::span<const ShaderToyImageResourceInfo> output_images, ShaderToyPassInfo & pass_info) noexcept;
asio::awaitable<std::expected<InstanceInfoPointer, std::error_code>> prepare_instance_info(
    const ShaderToyTaskContext & context, const InstanceInfoPointer & shader_toy_instance_info);

}
