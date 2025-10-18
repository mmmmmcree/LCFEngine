#pragma once

#include "InterleavedBuffer.h"
#include "enum_flags.h"
#include "enum_cast.h"
#include "PointerDefs.h"
#include "as_bytes.h"
#include "StructureLayout.h"
#include "BufferWriteSegment.h"
#include <magic_enum/magic_enum.hpp>
#include <array>

namespace lcf {
    enum class VertexSemanticFlags : uint16_t
    {
        eNone = 0,
        ePosition = 1 << 0,
        eNormal = 1 << 1,
        eTexCoord0 = 1 << 2,
        eTexCoord1 = 1 << 3,
        eTangent = 1 << 4,
        eBinormal = 1 << 5,
        eColor0 = 1 << 6,
    };
    MAKE_ENUM_FLAGS(VertexSemanticFlags);

    class Mesh
    {
        using Self = Mesh;
    public:
        using ByteList = std::vector<std::byte>;
        Mesh() = default;
        bool isCreated() const noexcept { return m_vertex_count > 0; }
        void create(size_t vertex_count);
        size_t getVertexCount() const noexcept { return m_vertex_count; }
        VertexSemanticFlags getVertexSemanticFlags() const noexcept { return m_vertex_semantic_flags; }
        bool hasSemantic(VertexSemanticFlags semantic_flags) const noexcept { return contains_flags(m_vertex_semantic_flags, semantic_flags); }
        Self & setVertexData(VertexSemanticFlags semantic_bit, ByteView attribute_data);
        ByteView getVertexDataSpan(VertexSemanticFlags semantic_bit) const noexcept;
        Self & clearVertexData(VertexSemanticFlags semantic_flags);
        template <integral_span_convertible_c SpanConvertible>
        Self & setIndexData(SpanConvertible && indices);
        ByteView getIndexDataSpan() const noexcept { return m_index_buffer; }
        template <integral_span_convertible_c SpanConvertible>
        Self & setFaces(SpanConvertible && faces);
        template <typename ShaderTypeMapping>
        BufferWriteSegments generateInterleavedVertexBufferSegments() const noexcept;
    private:
        size_t getAttributeIndex(VertexSemanticFlags semantic_bit) const noexcept { return std::countr_zero(to_integral(semantic_bit)); }
    private:
        size_t m_vertex_count = 0;
        size_t m_index_count = 0;
        std::array<ByteList, magic_enum::enum_count<VertexSemanticFlags>() - 1> m_vertex_data;
        ByteList m_index_buffer;
        std::vector<uint16_t> m_faces;
        VertexSemanticFlags m_vertex_semantic_flags = VertexSemanticFlags::eNone;
    };
}

namespace lcf {
    template <integral_span_convertible_c SpanConvertible>
    inline Mesh & Mesh::setIndexData(SpanConvertible &&indices)
    {
        auto indices_bytes = as_bytes(indices);
        m_index_buffer = ByteList(indices_bytes.begin(), indices_bytes.end());
        m_index_count = std::ranges::size(indices);
        return *this;   
    }

    template <integral_span_convertible_c SpanConvertible>
    inline Mesh & Mesh::setFaces(SpanConvertible &&faces)
    {
        m_faces = { std::ranges::begin(faces), std::ranges::end(faces) };
        return *this;
    }

    template <typename ShaderTypeMapping>
    inline BufferWriteSegments Mesh::generateInterleavedVertexBufferSegments() const noexcept
    {
        static const std::array<std::function<void(StructureLayout &)>, magic_enum::enum_count<VertexSemanticFlags>() - 1> s_add_field_methods = {
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // ePosition
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // eNormal
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec2_t>>(); }, // eTexCoord0
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec2_t>>(); }, // eTexCoord1
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // eTangent
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // eBinormal
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec4_t>>(); }, // eColor0
        };
        if (not this->isCreated()) { return {}; }
        StructureLayout layout;
        for (size_t i = 0, field_index = 0; i < m_vertex_data.size(); ++i) {
            if (m_vertex_data[i].empty()) { continue; }
            s_add_field_methods[i](layout);
        }
        layout.create();
        BufferWriteSegments segments;
        for (size_t i = 0, field_index = 0; i < m_vertex_data.size(); ++i) {
            if (m_vertex_data[i].empty()) { continue; }
            ByteView data_bytes = m_vertex_data[i];
            size_t data_stride = data_bytes.size_bytes() / m_vertex_count;
            size_t offset = layout.getFieldOffset(field_index++);
            size_t structural_size = layout.getStructualSize();
            for (size_t j = 0; j < m_vertex_count; ++j) {
                segments.add({data_bytes.subspan(j * data_stride, data_stride), offset + j * structural_size});
            }
        }
        return segments;
    }
}