#pragma once

#include <vulkan/vulkan.hpp>
#include "ShaderCore/Shader.h"
#include "PointerDefs.h"

namespace lcf::render {
	class VulkanContext;
	class VulkanShader : public Shader, public PointerDefs<VulkanShader>
	{
	public:
		IMPORT_POINTER_DEFS(VulkanShader);
		VulkanShader(VulkanContext * context, ShaderTypeFlagBits type);
		~VulkanShader() override = default;
		operator bool() const;
		virtual bool compileGlslFile(std::string_view file_path) override;
		virtual bool isCompiled() const override;
		vk::PipelineShaderStageCreateInfo getShaderStageInfo() const;
	private:
		VulkanContext * m_context = nullptr;
		vk::UniqueShaderModule m_module;
	};
}