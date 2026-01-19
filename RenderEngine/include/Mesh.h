#pragma once

#include "InterleavedBuffer.h"
#include "enum_flags.h"
#include "enum_cast.h"
#include "PointerDefs.h"
#include "bytes.h"
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
        eColor0 = 1 << 4,
        eColor1 = 1 << 5,
        eTangent = 1 << 6,
        eBinormal = 1 << 7,
        eJoints = 1 << 8,
        eWeights = 1 << 9,
        eAll = std::numeric_limits<std::underlying_type_t<VertexSemanticFlags>>::max()
    };
    LCF_MAKE_ENUM_FLAGS(VertexSemanticFlags);

    class Mesh : public STDPointerDefs<Mesh>
    {
        using Self = Mesh;
    public:
        using ByteList = std::vector<std::byte>;
        using IndexBuffer = std::vector<uint32_t>;
        using Face = std::pair<uint32_t, uint32_t>; // <index of IndexBuffer, index count>
        using FaceList = std::vector<Face>;
        Mesh() = default;
        bool isCreated() const noexcept { return m_vertex_count > 0; }
        void create(size_t vertex_count);
        Self & setName(std::string_view name) { m_name = name; return *this; }
        const std::string & getName() const noexcept { return m_name; }
        size_t getVertexCount() const noexcept { return m_vertex_count; }
        size_t getIndexCount() const noexcept { return m_index_count; }
        VertexSemanticFlags getVertexSemanticFlags() const noexcept { return m_vertex_semantic_flags; }
        bool hasSemantic(VertexSemanticFlags semantic_flags) const noexcept { return contains_flags(m_vertex_semantic_flags, semantic_flags); }
        Self & setVertexData(VertexSemanticFlags semantic_bit, ByteView attribute_data);
        ByteView getVertexDataSpan(VertexSemanticFlags semantic_bit) const noexcept;
        Self & clearVertexData(VertexSemanticFlags semantic_flags);
        template <integral_span_convertible_c SpanConvertible>
        Self & setIndexData(const SpanConvertible & indices);
        ByteView getIndexDataSpan() const noexcept { return as_bytes(m_index_buffer); }
        Self & addFace(const Face & face) { m_faces.emplace_back(face); return *this; }
        template <typename ShaderTypeMapping>
        BufferWriteSegments generateInterleavedVertexBufferSegments(VertexSemanticFlags enabled_flags = VertexSemanticFlags::eAll) const noexcept;
    private:
        size_t getAttributeIndex(VertexSemanticFlags semantic_bit) const noexcept { return std::countr_zero(std::to_underlying(semantic_bit)); }
    private:
        std::string m_name;
        size_t m_vertex_count = 0;
        size_t m_index_count = 0;
        std::array<ByteList, 10> m_vertex_data;
        IndexBuffer m_index_buffer;
        FaceList m_faces;
        VertexSemanticFlags m_vertex_semantic_flags = VertexSemanticFlags::eNone;
    };
}

namespace lcf {
    template <integral_span_convertible_c SpanConvertible>
    inline Mesh & Mesh::setIndexData(const SpanConvertible & indices)
    {
        m_index_buffer = IndexBuffer(std::ranges::begin(indices), std::ranges::end(indices));
        m_index_count = std::ranges::size(indices);
        return *this;   
    }

    template <typename ShaderTypeMapping>
    inline BufferWriteSegments Mesh::generateInterleavedVertexBufferSegments(VertexSemanticFlags enabled_flags) const noexcept
    {
        static const std::array<std::function<void(StructureLayout &)>, std::tuple_size_v<decltype(m_vertex_data)>> s_add_field_methods
        {
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // ePosition
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // eNormal
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec2_t>>(); }, // eTexCoord0
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec2_t>>(); }, // eTexCoord1
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec4_t>>(); }, // eColor0
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec4_t>>(); }, // eColor1
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // eTangent
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec3_t>>(); }, // eBinormal
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec4_t>>(); }, // eJoints
            [](StructureLayout & layout) { layout.addField<alignment_traits<typename ShaderTypeMapping::vec4_t>>(); }, // eWeights
        };
        auto is_index_skipped = [&](size_t i) -> bool
        {
            return m_vertex_data[i].empty() or not contains_flags(enabled_flags, static_cast<VertexSemanticFlags>(1 << i));
        };
        if (not this->isCreated()) { return {}; }
        StructureLayout layout;
        for (size_t i = 0, field_index = 0; i < m_vertex_data.size(); ++i) {
            if (is_index_skipped(i)) { continue; }
            s_add_field_methods[i](layout);
        }
        layout.create();
        BufferWriteSegments segments;
        for (size_t i = 0, field_index = 0; i < m_vertex_data.size(); ++i) {
            if (is_index_skipped(i)) { continue; }
            ByteView data_bytes = m_vertex_data[i];
            size_t data_stride = data_bytes.size_bytes() / m_vertex_count;
            size_t type_size = std::min(data_stride, layout.getFieldAlignedSize(field_index));
            size_t offset = layout.getFieldOffset(field_index);
            size_t structural_size = layout.getStructualSize();
            for (size_t j = 0; j < m_vertex_count; ++j) {
                segments.add({data_bytes.subspan(j * data_stride, type_size), offset + j * structural_size});
            }
            ++field_index;
        }
        return segments;
    }
}