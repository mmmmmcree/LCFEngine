#include "VulkanPushConstant.h"
#include <numeric>


void lcf::render::VulkanPushConstant::bind(vk::CommandBuffer cmd, vk::PipelineLayout layout) const
{
    for (const auto &push_constant_data : m_data_list) {
        cmd.pushConstants(layout, m_stage_flags, m_offset + push_constant_data.offset, push_constant_data.size, push_constant_data.data);
    }
}

lcf::render::VulkanPushConstant &lcf::render::VulkanPushConstant::setDataLayout(uint32_t index, uint32_t offset, uint32_t size)
{
    if (index >= m_data_list.size()) {
        m_data_list.resize(index + 1);
    }
    m_data_list[index].offset = offset;
    m_data_list[index].size = size;
    return *this;
}

lcf::render::VulkanPushConstant & lcf::render::VulkanPushConstant::setData(uint32_t index, const void *data)
{
    if (index >= m_data_list.size()) { return *this; }
    m_data_list[index].data = data;
    return *this;
}

lcf::render::VulkanPushConstant & lcf::render::VulkanPushConstant::setData(std::span<const void *> data_list)
{
    size_t n = std::min(m_data_list.size(), data_list.size());
    for (size_t i = 0; i < n; ++i) {
        m_data_list[i].data = data_list[i];
    }
    return *this;
}

lcf::render::VulkanPushConstant &lcf::render::VulkanPushConstant::setData(const std::initializer_list<const void *> &data_list)
{
    std::vector<const void *> data_vec(data_list);
    this->setData(data_vec);
    return *this;
}

uint32_t lcf::render::VulkanPushConstant::getSize() const
{
    return std::transform_reduce(m_data_list.begin(), m_data_list.end(), 0u, std::plus<uint32_t>(), [](const auto &data) { return data.size; });
}