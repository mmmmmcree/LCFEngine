#pragma once

#include "render_assets_enums.h"
#include "BufferWriteSegment.h"
#include "Vector.h"
#include "StructureLayout.h"
#include "concepts/standard_layout_concept.h"
#include "lcf_type_traits.h"
#include "PointerDefs.h"
#include <vector>
#include <array>
#include <magic_enum/magic_enum.hpp>

namespace lcf {
    template <typename T>
    concept vertex_attribute_type_bridge_c = requires {
        typename T::vec2_t;
        typename T::vec3_t;
        typename T::vec4_t;
    };

    class Geometry : public STDPointerDefs<Geometry>
    {
        using Self = Geometry;
    public:
        inline static constexpr size_t ATTRIBUTE_COUNT = magic_enum::enum_count<VertexSemantic>();
        using ByteList = std::vector<std::byte>;
        using VertexAttributesMap = std::array<ByteList, ATTRIBUTE_COUNT>;
        using IndexList = std::vector<uint32_t>;
        using Face = std::pair<size_t, size_t>; // <index of IndexBuffer, index count>
        using FaceList = std::vector<Face>;
    public:
        Geometry() = default;
        ~Geometry() = default;
        Geometry(const Self &) = default;
        Self & operator=(const Self &) = default;
        Geometry(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        void create(size_t vertex_count);
        bool isCreated() const noexcept { return m_vertex_count; }
        template <vertex_attribute_type_bridge_c TypeBridge>
        BufferWriteSegments generateInterleavedVertexBufferSegments(VertexSemanticFlags enabled_flags = VertexSemanticFlags::eAll) const noexcept;
        Self & setVertexAttributes(VertexSemantic semantic, ByteView attributes);
        Self & clearVertexAttributes(VertexSemantic semantic);
        Self & clearVertexAttributes(VertexSemanticFlags semantic_flags);
        template <integral_span_convertible_c SpanConvertible>
        Self & setIndices(SpanConvertible indices) noexcept { m_indices.assign_range(indices); return *this; }
        Self & addFace(Face face) { m_faces.emplace_back(face); return *this; }
        VertexSemanticFlags getSemanticFlags() const noexcept { return m_vertex_semantic_flags; }
        bool hasSemanticFlags(VertexSemanticFlags semantic_flags) const noexcept;
        size_t getVertexCount() const noexcept { return m_vertex_count; }
        size_t getIndexCount() const noexcept { return m_indices.size(); }
        size_t getAttributeSize(VertexSemantic semantic) const noexcept;
        std::span<const uint32_t> getIndices() const noexcept { return m_indices; }

        template <standard_layout_c T>
        const T * getVertexAttribute(VertexSemantic semantic, size_t vertex_index) const noexcept;
        template<standard_layout_c T>
        T * getVertexAttribute(VertexSemantic semantic, size_t vertex_index) noexcept;
        template<standard_layout_c T>
        std::span<const T> getVertexAttributeView(VertexSemantic semantic) const noexcept;
        template<standard_layout_c T>
        std::span<T> getVertexAttributeView(VertexSemantic semantic) noexcept;
        template <standard_layout_c T>
        void setVertexAttribute(VertexSemantic semantic, size_t vertex_index, const T & attribute) noexcept;
    private:
        void addSemanticFlag(VertexSemantic semantic) noexcept;
        void removeSemanticFlag(VertexSemantic semantic) noexcept;
        ByteList & getVertexAttributes(VertexSemantic semantic) noexcept;
        const ByteList & getVertexAttributes(VertexSemantic semantic) const noexcept;
        template <standard_layout_c T>
        bool isTypeValid(VertexSemantic semantic) const noexcept;
    private:
        size_t m_vertex_count = 0;
        VertexSemanticFlags m_vertex_semantic_flags = VertexSemanticFlags::eNone;
        VertexAttributesMap m_vertex_attributes_map;
        IndexList m_indices;
        FaceList m_faces;
    };
    
}

template <lcf::vertex_attribute_type_bridge_c TypeBridge>
inline lcf::BufferWriteSegments lcf::Geometry::generateInterleavedVertexBufferSegments(VertexSemanticFlags enabled_flags) const noexcept
{
    using vec2_t = typename TypeBridge::vec2_t;
    using vec3_t = typename TypeBridge::vec3_t;
    using vec4_t = typename TypeBridge::vec4_t;
    static const std::array<std::function<void(StructureLayout &)>, ATTRIBUTE_COUNT> s_add_field_methods
    {
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec3_t>>(); }, // ePosition
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec3_t>>(); }, // eNormal
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec2_t>>(); }, // eTexCoord0
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec2_t>>(); }, // eTexCoord1
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec4_t>>(); }, // eColor0
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec4_t>>(); }, // eColor1
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec3_t>>(); }, // eTangent
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec3_t>>(); }, // eBinormal
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec4_t>>(); }, // eJoints
        [](StructureLayout & layout) { layout.addField<alignment_traits<vec4_t>>(); }, // eWeights
    };
    auto is_index_skipped = [this, enabled_flags](size_t i) -> bool
    {
        return m_vertex_attributes_map[i].empty() or not contains_flags(enabled_flags, static_cast<VertexSemanticFlags>(1 << i));
    };
    if (not this->isCreated()) { return {}; }
    StructureLayout layout;
    for (size_t i = 0; i < ATTRIBUTE_COUNT; ++i) {
        if (is_index_skipped(i)) { continue; }
        s_add_field_methods[i](layout);
    }
    layout.create();
    BufferWriteSegments segments;
    for (size_t i = 0, field_index = 0; i < ATTRIBUTE_COUNT; ++i) {
        if (is_index_skipped(i)) { continue; }
        ByteView data_bytes = m_vertex_attributes_map[i];
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

template <lcf::standard_layout_c T>
inline const T *lcf::Geometry::getVertexAttribute(
    VertexSemantic semantic,
    size_t vertex_index) const noexcept
{
    if (not this->isTypeValid<T>(semantic) or vertex_index >= m_vertex_count) { return nullptr; }
    return reinterpret_cast<const T *>(this->getVertexAttributes(semantic).data() + vertex_index * this->getAttributeSize(semantic));
}

template <lcf::standard_layout_c T>
inline T * lcf::Geometry::getVertexAttribute(VertexSemantic semantic, size_t vertex_index) noexcept
{
    if (not this->isTypeValid<T>(semantic) or vertex_index >= m_vertex_count) { return nullptr; }
    return reinterpret_cast<T *>(this->getVertexAttributes(semantic).data() + vertex_index * this->getAttributeSize(semantic));
}

template <lcf::standard_layout_c T>
inline std::span<const T> lcf::Geometry::getVertexAttributeView(VertexSemantic semantic) const noexcept
{
    return bytes_as_span<T>(as_bytes(this->getVertexAttributes(semantic)));
}

template <lcf::standard_layout_c T>
inline std::span<T> lcf::Geometry::getVertexAttributeView(VertexSemantic semantic) noexcept
{
    return bytes_as_span<T>(as_bytes(this->getVertexAttributes(semantic)));
}

template <lcf::standard_layout_c T>
inline void lcf::Geometry::setVertexAttribute(
    VertexSemantic semantic,
    size_t vertex_index,
    const T &attribute) noexcept
{
    T * attribute_p = this->getVertexAttribute<T>(semantic, vertex_index);
    if (attribute_p) {
        *attribute_p = attribute;
    }
}

template <lcf::standard_layout_c T>
inline bool lcf::Geometry::isTypeValid(VertexSemantic semantic) const noexcept
{
    return size_of_v<T> <= this->getAttributeSize(semantic);
}
