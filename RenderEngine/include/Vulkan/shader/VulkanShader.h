#pragma once

#include "shader_core/ShaderResource.h"
#include "shader_core/spirv.h"
#include "Vulkan/vulkan_fwd_decls.h"
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
		Self & setUnit(const sc::spirv::Unit & spv_unit) noexcept { m_spv_unit = spv_unit; return *this; }
		std::error_code create(vk::Device device) noexcept;
		bool isCreated() const noexcept { return m_module.get(); }
        ShaderTypeFlagBits getStage() const noexcept { return m_spv_unit.getStage(); }
		const ShaderResources & getResources() const noexcept { return m_resources; }
		const LayoutMap & getDescriptorLayouts() const noexcept { return m_layout_map; }
		vk::PipelineShaderStageCreateInfo getShaderStageInfo() const noexcept;
	private:
		sc::spirv::Unit m_spv_unit;
		ShaderResources m_resources;
		LayoutMap m_layout_map;
		vk::UniqueShaderModule m_module;
	};
}