#pragma once

#include "vulkan_fwd_decls.h"
#include <vulkan/vulkan.hpp>
#include "shader_core/Shader.h"

namespace lcf::render {
	class VulkanShader : public Shader, public VulkanShaderPointerDefs
	{
	public:
		IMPORT_POINTER_DEFS(VulkanShaderPointerDefs);
		VulkanShader(VulkanContext * context, ShaderTypeFlagBits type);
		~VulkanShader() override;
		VulkanShader(const VulkanShader & other) = delete;
		VulkanShader& operator=(const VulkanShader & other) = delete;
		VulkanShader(VulkanShader && other) noexcept = default;
		VulkanShader& operator=(VulkanShader && other) noexcept = default;
		operator bool() const;
		virtual bool compileGlslFile(const std::filesystem::path & file_path) override;
		virtual bool isCompiled() const override;
		vk::PipelineShaderStageCreateInfo getShaderStageInfo() const;
	private:
		VulkanContext * m_context_p = nullptr;
		vk::UniqueShaderModule m_module;
		std::vector<vk::DescriptorSetLayout> m_descriptor_set_layout_list;
	};
}