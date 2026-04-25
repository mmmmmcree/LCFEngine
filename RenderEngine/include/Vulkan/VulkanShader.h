#pragma once

#include "shader_core/shader_core_fwd_decls.h"
#include "shader_core/ShaderResource.h"
#include "shader_core/shader_utils.h"
#include "vulkan_fwd_decls.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout.h"
#include <vulkan/vulkan.hpp>
#include <filesystem>
#include <unordered_map>

namespace lcf::render {
	class VulkanShader
	{
		using Self = VulkanShader;
		using LayoutMap = std::unordered_map<uint32_t, VulkanDescriptorSetLayout>;
	public:
		VulkanShader() = default;
		~VulkanShader() noexcept = default;
		VulkanShader(const VulkanShader & other) = delete;
		VulkanShader& operator=(const VulkanShader & other) = delete;
		VulkanShader(VulkanShader && other) noexcept = default;
		VulkanShader& operator=(VulkanShader && other) noexcept = default;
		operator bool() const noexcept { return this->isCreated(); }
	public:
		Self & compileGlslFile(
			ShaderTypeFlagBits type,
			const std::filesystem::path & file_path,
			std::string_view entry_point = "main") noexcept;
		std::error_code create(vk::Device device) noexcept;
		bool isCreated() const noexcept { return m_module.get(); }
        ShaderTypeFlagBits getStage() const noexcept { return m_stage; }
		const ShaderResources & getResources() const noexcept { return m_resources; }
		const LayoutMap & getDescriptorLayouts() const noexcept { return m_layout_map; }
		vk::PipelineShaderStageCreateInfo getShaderStageInfo() const noexcept;
	private:
        ShaderTypeFlagBits m_stage;
		std::string m_entry_point;
		spirv::Code m_spv_code;
		ShaderResources m_resources;
		LayoutMap m_layout_map;
		vk::UniqueShaderModule m_module;
	};
}