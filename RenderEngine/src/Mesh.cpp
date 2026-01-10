#include "Mesh.h"

using namespace lcf;

void Mesh::create(size_t vertex_count)
{
    if (this->isCreated()) { return; }
    m_vertex_count = vertex_count;
}

Mesh & Mesh::setVertexData(VertexSemanticFlags semantic_bit, ByteView attribute_data)
{
    if (not is_flag_bit(semantic_bit)) { return *this; }
    if (attribute_data.empty()) {
        return this->clearVertexData(semantic_bit);
    }
    m_vertex_data[this->getAttributeIndex(semantic_bit)] = ByteList(attribute_data.begin(), attribute_data.end());
    m_vertex_semantic_flags |= semantic_bit;
    return *this;
}

ByteView Mesh::getVertexDataSpan(VertexSemanticFlags semantic_bit) const noexcept
{
    if (not is_flag_bit(semantic_bit)) { return {}; }
    return m_vertex_data[this->getAttributeIndex(semantic_bit)];
}

Mesh & Mesh::clearVertexData(VertexSemanticFlags semantic_flags)
{
    auto semantic_value = std::to_underlying(semantic_flags);
    while (semantic_value != 0) {
        size_t attribute_index = std::countr_zero(semantic_value);
        m_vertex_data[attribute_index].clear();
        m_vertex_semantic_flags &= ~static_cast<VertexSemanticFlags>(1 << attribute_index);
        semantic_value &= (semantic_value - 1);
    }
    return *this;
}