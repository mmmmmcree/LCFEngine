#pragma once

#include "shader_core/shader_core_fwd_decls.h"
#include "shader_core/ShaderResource.h"
#include "vulkan_fwd_decls.h"
#include <vulkan/vulkan.hpp>
#include "JSON.h"

namespace lcf::render {
	class VulkanShader : public VulkanShaderPointerDefs
	{
	public:
		VulkanShader(VulkanContext * context, ShaderTypeFlagBits type, std::string_view entry_point = "main");
		~VulkanShader();
		VulkanShader(const VulkanShader & other) = delete;
		VulkanShader& operator=(const VulkanShader & other) = delete;
		VulkanShader(VulkanShader && other) noexcept = default;
		VulkanShader& operator=(VulkanShader && other) noexcept = default;
		operator bool() const;
        ShaderTypeFlagBits getStage() const noexcept { return m_stage; }
		const ShaderResources & getResources() const { return m_resources; }
		const JSON & getPragmas() const { return m_pragmas; }
		std::error_code compileGlslFile(const std::filesystem::path & file_path);
		bool isCompiled() const;
		vk::PipelineShaderStageCreateInfo getShaderStageInfo() const;
	private:
		VulkanContext * m_context_p = nullptr;
        ShaderTypeFlagBits m_stage;
		std::string m_entry_point;
		ShaderResources m_resources;
		JSON m_pragmas;
		vk::UniqueShaderModule m_module;
		std::vector<vk::DescriptorSetLayout> m_descriptor_set_layout_list;
	};
}