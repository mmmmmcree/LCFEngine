#include "Mesh.h"
#include "Vector.h"
#include "lcf_type_traits.h"
#include "common/glsl_alignment_traits.h"

using namespace lcf;

Mesh & Mesh::addSemantic(VertexSemanticFlags semantic_flags) noexcept
{
    if (this->isCreated()) { return *this; } 
    m_vertex_semantic_flags |= semantic_flags;
    return *this;
}