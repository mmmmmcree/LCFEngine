#include "VulkanMaterial.h"
#include "VulkanContext.h"
#include "VulkanDescriptorWriter.h"

using namespace lcf::render;

bool lcf::render::VulkanMaterial::create(VulkanContext *context_p, BindingViewList bindings)
{
    m_context_p = context_p;
    auto device = context_p->getDevice();
    m_bindings = BindingList(bindings.begin(), bindings.end());
    
    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.setBindings(m_bindings);
    m_descriptor_set_layout = device.createDescriptorSetLayoutUnique(layout_info);
    auto & descriptor_manager = m_context_p->getDescriptorManager();
    m_descriptor_set = descriptor_manager.allocateUnique(m_descriptor_set_layout.get());
    for (const auto & binding_info : m_bindings) {
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
    return m_descriptor_set.get();
}

VulkanMaterial & lcf::render::VulkanMaterial::setTexture(uint32_t binding, uint32_t index, const VulkanImage::SharedPointer &texture)
{
    if (binding >= m_bindings.size()) { return *this; }
    const auto & binding_info = m_bindings[binding];
    auto desc_type = binding_info.descriptorType;
    if (not (desc_type == vk::DescriptorType::eCombinedImageSampler or desc_type == vk::DescriptorType::eSampledImage)) { return *this; }
    auto & texture_list = m_texture_map[binding];
    if (index >= texture_list.size()) { return *this; }
    texture_list[index] = texture;
    m_dirty_bindings[binding] = true;
    return *this;
}

VulkanMaterial & lcf::render::VulkanMaterial::setSampler(uint32_t binding, uint32_t index, const VulkanSampler::SharedPointer &sampler)
{
    if (binding >= m_bindings.size()) { return *this; }
    const auto & binding_info = m_bindings[binding];
    auto desc_type = binding_info.descriptorType;
    if (not (desc_type == vk::DescriptorType::eCombinedImageSampler or desc_type == vk::DescriptorType::eSampler)) { return *this; }
    auto & sampler_list = m_sampler_map[binding];
    if (index >= sampler_list.size()) { return *this; }
    sampler_list[index] = sampler;
    m_dirty_bindings[binding] = true;
    return *this;
}

void lcf::render::VulkanMaterial::commitUpdate()
{
    VulkanDescriptorWriter writer(m_context_p, m_bindings);
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, vk::DescriptorImageInfo>> image_infos;
    for (uint32_t binding = 0; binding < m_bindings.size(); ++binding) {
        if (not m_dirty_bindings[binding]) { continue; }
        const auto & samplers = m_sampler_map[binding];
        auto descriptor_type = m_bindings[binding].descriptorType;
        for (uint32_t i = 0; i < samplers.size(); ++i) {
            const auto & sampler_sp = samplers[i];
            if (not sampler_sp or not sampler_sp->isCreated()) { continue; }
            auto & image_info = image_infos[binding][i];
            image_info.setSampler(sampler_sp->getHandle());
            if (descriptor_type == vk::DescriptorType::eCombinedImageSampler) { continue; }
            writer.add(binding, i, image_info);
        }
    }
    for (uint32_t binding = 0; binding < m_bindings.size(); ++binding) {
        if (not m_dirty_bindings[binding]) { continue; }
        const auto & textures = m_texture_map[binding];
        for (uint32_t index = 0; index < textures.size(); index++) {
            auto & texture_sp = textures[index];
            if (not texture_sp or not texture_sp->isCreated()) { continue; }
            auto & image_info = image_infos[binding][index];
            image_info.setImageLayout(*texture_sp->getLayout())
                .setImageView(texture_sp->getDefaultView());
            writer.add(binding, index, image_info);
        }
    }
    m_dirty_bindings.reset();
    writer.write(m_descriptor_set.get());
}
