#pragma once

#include "InterleavedBuffer.h"
#include "enum_flags.h"
#include "enum_cast.h"
#include "PointerDefs.h"
#include "as_bytes.h"

namespace lcf {
    enum class VertexSemanticFlags : uint16_t
    {
        ePosition = 1,
        eNormal = 1 << 1,
        eTexCoord0 = 1 << 2,
        eTexCoord1 = 1 << 3,
        eTangent = 1 << 4,
        eBinormal = 1 << 5,
        eColor0 = 1 << 6,
    };
    MAKE_ENUM_FLAGS(VertexSemanticFlags);

    class Mesh : public STDPointerDefs<Mesh>
    {
        using Self = Mesh;
    public:
        using IndexBuffer = std::vector<std::byte>;
        using FaceBuffer = std::vector<uint16_t>;
        bool isCreated() const noexcept { return m_vertex_buffer.isCreated(); }
        Self & addSemantic(VertexSemanticFlags semantic_flags) noexcept;
        bool hasSemantic(VertexSemanticFlags semantic_flags) const noexcept { return contains_flags(m_vertex_semantic_flags, semantic_flags); }
        VertexSemanticFlags getVertexSemanticFlags() const noexcept { return m_vertex_semantic_flags; }
        template <typename ShaderTypeMapping> void create(size_t vertex_count);
        template <std::ranges::range Range>
        Self & setVertexData(VertexSemanticFlags semantic_bit, Range && data, size_t start_index = 0);
        Self & setIndexData(ByteView indices) { m_index_buffer = IndexBuffer(indices.begin(), indices.end()); return *this; }
        template <std::integral Integral>
        Self & setFaceData(std::span<const Integral> faces) { m_faces = FaceBuffer(faces.begin(), faces.end()); return *this; }
        size_t getVertexCount() const noexcept { return m_vertex_buffer.getSize(); }
        ByteView getVertexDataSpan() const noexcept { return m_vertex_buffer.getDataSpan(); }
        ByteView getIndexDataSpan() const noexcept { return as_bytes(m_index_buffer); }
        ByteView getFaceDataSpan() const noexcept { return as_bytes(m_faces); }
    private:
        InterleavedBuffer m_vertex_buffer;
        IndexBuffer m_index_buffer;
        FaceBuffer m_faces;
        VertexSemanticFlags m_vertex_semantic_flags;
    };
}

namespace lcf {
    template <typename ShaderTypeMapping>
    inline void Mesh::create(size_t vertex_count)
    {
        if (this->isCreated()) { return; }
        if (this->hasSemantic(VertexSemanticFlags::ePosition)) {
            m_vertex_buffer.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>();
        }
        if (this->hasSemantic(VertexSemanticFlags::eNormal)) {
            m_vertex_buffer.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>();
        }
        if (this->hasSemantic(VertexSemanticFlags::eTexCoord0)) {
            m_vertex_buffer.addField<alignment_traits<typename ShaderTypeMapping::vec2_t>>();
        }
        if (this->hasSemantic(VertexSemanticFlags::eTexCoord1)) {
            m_vertex_buffer.addField<alignment_traits<typename ShaderTypeMapping::vec2_t>>();
        }
        m_vertex_buffer.create(vertex_count);
    }

    template<std::ranges::range Range>
    inline Mesh & Mesh::setVertexData(VertexSemanticFlags semantic_bit, Range && data, size_t start_index)
    {
        if (not this->isCreated() or not is_flag_bit(semantic_bit) or not this->hasSemantic(semantic_bit)) { return *this; }
        size_t field_index = std::popcount<size_t>(enum_cast(m_vertex_semantic_flags) & (enum_cast(semantic_bit) - 1));
        m_vertex_buffer.setData(field_index, std::forward<Range>(data), start_index);
        return *this;
    }
}