#include "Geometry.h"
#include "enum_cast.h"

using namespace lcf;

void Geometry::create(size_t vertex_count)
{
    if (this->isCreated()) { return; }
    m_vertex_count = vertex_count;
}

Geometry & Geometry::setVertexAttributes(VertexSemantic semantic, ByteView attributes)
{
    if (not this->isCreated()) { return *this; }
    if (attributes.empty()) {
        this->clearVertexAttributes(semantic);
    } else {
        this->getVertexAttributes(semantic).assign_range(attributes);
        this->addSemanticFlag(semantic);
    }
    return *this;
}

Geometry & Geometry::clearVertexAttributes(VertexSemantic semantic)
{
    this->removeSemanticFlag(semantic);
    this->getVertexAttributes(semantic).clear();
    return *this;
}

Geometry & Geometry::clearVertexAttributes(VertexSemanticFlags semantic_flags)
{
    auto semantic_flags_value = to_integral(semantic_flags);
    while (semantic_flags_value != 0) {
        size_t attribute_index = std::countr_zero(semantic_flags_value);
        this->clearVertexAttributes(static_cast<VertexSemantic>(attribute_index));
        semantic_flags_value &= (semantic_flags_value - 1);
    }
    return *this;
}

bool lcf::Geometry::hasSemanticFlags(VertexSemanticFlags semantic_flags) const noexcept
{
    return contains_flags(m_vertex_semantic_flags, semantic_flags);
}

size_t lcf::Geometry::getAttributeSize(VertexSemantic semantic) const noexcept
{
    return this->getVertexAttributes(semantic).size() / this->getVertexCount();
}

void lcf::Geometry::addSemanticFlag(VertexSemantic semantic) noexcept
{
    m_vertex_semantic_flags |= static_cast<VertexSemanticFlags>(1 << (to_integral(semantic) + 1));
}

void lcf::Geometry::removeSemanticFlag(VertexSemantic semantic) noexcept
{
    m_vertex_semantic_flags &= ~static_cast<VertexSemanticFlags>(1 << (to_integral(semantic) + 1));
}

Geometry::ByteList &Geometry::getVertexAttributes(VertexSemantic semantic) noexcept
{
    return m_vertex_attributes_map[to_integral(semantic)];
}

const Geometry::ByteList & Geometry::getVertexAttributes(VertexSemantic semantic) const noexcept
{
    return m_vertex_attributes_map[to_integral(semantic)];
}
