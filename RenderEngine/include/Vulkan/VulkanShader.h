#pragma once

#include <vulkan/vulkan.hpp>
#include "ShaderCore/Shader.h"
#include "PointerDefs.h"

namespace vk { class DispatchLoaderDynamic; }
namespace lcf::render {
	class VulkanContext;
	class VulkanShader : public Shader, public STDPointerDefs<VulkanShader>
	{
	public:
		IMPORT_POINTER_DEFS(STDPointerDefs<VulkanShader>);
		VulkanShader(VulkanContext * context, ShaderTypeFlagBits type);
		VulkanShader(const VulkanShader& other) = delete;
		VulkanShader& operator=(const VulkanShader& other) = delete;
		~VulkanShader() override;
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