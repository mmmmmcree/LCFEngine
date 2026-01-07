#include "Vulkan/VulkanMaterial.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

bool lcf::render::VulkanMaterial::create(const VulkanDescriptorSet::SharedPointer & descriptor_set_sp)
{
    m_descriptor_set_sp = descriptor_set_sp;
     for (const auto & binding_info : m_descriptor_set_sp->getLayout().getBindings()) {
        uint32_t binding = binding_info.binding;
        size_t count = binding_info.descriptorCount;
        switch (binding_info.descriptorType) {
            case vk::DescriptorType::eCombinedImageSampler: {
                m_sampler_map[binding].resize(count);
            } [[fallthrough]];
            case vk::DescriptorType::eSampledImage: {
                m_texture_map[binding].resize(count); //todo create default texture
            } break;
            case vk::DescriptorType::eSampler: {
                m_sampler_map[binding].resize(count);
            } break;
        }
    }
    return this->getDescriptorSet().getHandle();
}

VulkanMaterial & VulkanMaterial::setTexture(uint32_t binding, uint32_t index, const VulkanImageObject::SharedPointer &texture)
{
    auto bindings = m_descriptor_set_sp->getLayout().getBindings();
    if (binding >= bindings.size()) { return *this; }
    const auto & binding_info = bindings[binding];
    auto desc_type = binding_info.descriptorType;
    if (not (desc_type == vk::DescriptorType::eCombinedImageSampler or desc_type == vk::DescriptorType::eSampledImage)) { return *this; }
    auto & texture_list = m_texture_map[binding];
    if (index >= texture_list.size()) { return *this; }
    texture_list[index] = texture;
    m_dirty_bindings[binding] = true;
    return *this;
}

VulkanMaterial & VulkanMaterial::setSampler(uint32_t binding, uint32_t index, const VulkanSampler::SharedPointer &sampler)
{
    auto bindings = m_descriptor_set_sp->getLayout().getBindings();
    if (binding >= bindings.size()) { return *this; }
    const auto & binding_info = bindings[binding];
    auto desc_type = binding_info.descriptorType;
    if (not (desc_type == vk::DescriptorType::eCombinedImageSampler or desc_type == vk::DescriptorType::eSampler)) { return *this; }
    auto & sampler_list = m_sampler_map[binding];
    if (index >= sampler_list.size()) { return *this; }
    sampler_list[index] = sampler;
    m_dirty_bindings[binding] = true;
    return *this;
}

VulkanMaterial & VulkanMaterial::setParamsSSBO(uint32_t binding, const VulkanBufferObject::SharedPointer &params_ssbo)
{
    m_dirty_bindings[binding] = true;
    m_binding_params_ssbo = std::make_pair(binding, params_ssbo);
    return *this;
}
void VulkanMaterial::commitUpdate()
{
    auto updater = m_descriptor_set_sp->generateUpdater();
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, vk::DescriptorImageInfo>> image_infos;
    auto bindings = m_descriptor_set_sp->getLayout().getBindings();
    for (uint32_t binding = 0; binding < bindings.size(); ++binding) {
        if (not m_dirty_bindings[binding]) { continue; }
        const auto & samplers = m_sampler_map[binding];
        auto descriptor_type = bindings[binding].descriptorType;
        for (uint32_t index = 0; index < samplers.size(); ++index) {
            const auto & sampler_sp = samplers[index];
            if (not sampler_sp or not sampler_sp->isCreated()) { continue; }
            auto & sampler_info = image_infos[binding][index];
            sampler_info.setSampler(sampler_sp->getHandle());
            if (descriptor_type == vk::DescriptorType::eCombinedImageSampler) { continue; }
            updater.add(binding, index, sampler_info);
        }
    }
    for (uint32_t binding = 0; binding < bindings.size(); ++binding) {
        if (not m_dirty_bindings[binding]) { continue; }
        const auto & textures = m_texture_map[binding];
        for (uint32_t index = 0; index < textures.size(); index++) {
            auto & texture_sp = textures[index];
            if (not texture_sp or not texture_sp->isCreated()) { continue; }
            auto & image_info = image_infos[binding][index];
            image_info.setImageLayout(*texture_sp->getLayout())
                .setImageView(texture_sp->getDefaultView());
            updater.add(binding, index, image_info);
        }
    }
    // i
    // auto
    const auto & [params_binding, ssbo_sp] = m_binding_params_ssbo;
    if (params_binding < s_max_binding_count and m_dirty_bindings[params_binding] and ssbo_sp) {
        vk::DescriptorBufferInfo per_renderable_vertex_buffer_info(ssbo_sp->getHandle(), 0, vk::WholeSize);
        updater.add(params_binding, per_renderable_vertex_buffer_info);
    }
    m_dirty_bindings.reset();
    updater.update();
}
