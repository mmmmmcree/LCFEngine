#pragma once

#include "render_assets_fwd_decls.h"
#include "render_assets_enums.h"
#include "enums/enum_count.h"
#include "vector_enum_value_types.h"
#include "span_cast.h"
#include "bytes.h"
#include "type_traits/lcf_type_traits.h"
#include "BufferWriteSegment.h"
#include "StructureLayout.h"
#include "PointerDefs.h"
#include <vector>
#include <array>
#include <ranges>

namespace lcf {
    class Geometry : public GeometryPointerDefs
    {
        using Self = Geometry;
        template <VertexAttribute attribute>
        using attribute_basic_t = enum_value_t<enum_decode::get_basic_data_type(attribute)>;
        template <VertexAttribute attribute>
        using attribute_t = enum_value_t<enum_decode::get_vector_type(attribute)>;
    public:
        using ByteList = std::vector<std::byte>;
        static constexpr size_t c_vertex_attribute_count = enum_count_v<internal::VertexAttribute>;
        using AttributesMap = std::array<ByteList, c_vertex_attribute_count>;
        using IndexList = std::vector<uint32_t>;
        using Face = std::pair<uint32_t, uint32_t>; // <index of IndexBuffer, index count>
        using FaceList = std::vector<Face>;
    public:
        Geometry() noexcept = default;
        explicit Geometry(uint32_t vertex_count) noexcept : m_vertex_count(vertex_count) {}
        ~Geometry() noexcept = default;
        Geometry(const Self &) = default;
        Self & operator=(const Self &) = default;
        Geometry(Self &&) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
    public:
        Self & resize(uint32_t vertex_count)
        {
            m_vertex_count = vertex_count;
            for (auto && [i, attributes_bytes] : std::ranges::views::enumerate(m_attributes_map)) {
                if (attributes_bytes.empty()) { continue; }
                attributes_bytes.resize(m_vertex_count * enum_decode::get_size_in_bytes(enum_decode::get_vector_type(enum_values_v<VertexAttribute>[i])));
            }
            return *this;
        }
        template <VertexAttribute attribute, std::ranges::contiguous_range Range>
        requires is_convertible_v<std::ranges::range_value_t<Range>, attribute_basic_t<attribute>> or
            is_pointer_type_convertible_v<std::ranges::range_value_t<Range>, attribute_basic_t<attribute>> or
            is_convertible_v<std::ranges::range_value_t<Range>, attribute_t<attribute>> or
            is_pointer_type_convertible_v<std::ranges::range_value_t<Range>, attribute_t<attribute>>
        Self & setAttributes(Range && range, size_t start_index = 0)
        {
            using attribute_basic_type = attribute_basic_t<attribute>;
            using attribute_type = attribute_t<attribute>;
            using src_t = std::ranges::range_value_t<Range>;
            this->ensureAttributeExist(attribute);
            if constexpr (is_convertible_v<src_t, attribute_type>) {
                auto attributes = this->getAttributes<attribute>();
                std::ranges::copy(std::forward<Range>(range), attributes.begin() + start_index);
            } else if constexpr (is_pointer_type_convertible_v<src_t, attribute_type>) {
                auto attributes = this->getAttributes<attribute>();
                std::ranges::copy(span_cast<attribute_type>(std::forward<Range>(range)), attributes.begin() + start_index);
            } else if constexpr (is_convertible_v<src_t, attribute_basic_type>) {
                auto attributes = this->getBasicAttributes<attribute>();
                std::ranges::copy(std::forward<Range>(range), attributes.begin() + start_index * enum_decode::get_vector_dimension(enum_decode::get_vector_type(attribute)));
            } else if constexpr (is_pointer_type_convertible_v<src_t, attribute_basic_type>) {
                auto attributes = this->getBasicAttributes<attribute>();
                auto span = span_cast<attribute_basic_type>(std::forward<Range>(range));
                std::ranges::copy(span, attributes.begin() + start_index * enum_decode::get_vector_dimension(enum_decode::get_vector_type(attribute)));
            }
            return *this;
        }
        template <std::ranges::contiguous_range Range>
        Self & setRawAttributes(VertexAttribute attribute, Range && range, size_t start_index = 0)
        {
            this->ensureAttributeExist(attribute);
            auto & attributes_bytes = m_attributes_map[enum_decode::get_index(attribute)];
            size_t offset = start_index * enum_decode::get_size_in_bytes(enum_decode::get_vector_type(attribute));
            std::ranges::copy(as_bytes(std::forward<Range>(range)), attributes_bytes.begin() + offset);
            return *this;
        }
        template <VertexAttribute attribute>
        Self & clearAttributes() noexcept
        {
            m_attributes_map[enum_decode::get_index(attribute)] = ByteList {};
            return *this;
        }
        template <VertexAttribute attribute>
        Self & setAttribute(size_t index, const attribute_t<attribute> & value)
        {
            this->ensureAttributeExist(attribute);
            this->getAttribute<attribute>(index) = value;
            return *this;
        }
        std::span<const std::byte> getRawAttributes(VertexAttribute attribute) const noexcept { return m_attributes_map[enum_decode::get_index(attribute)]; }
        template <VertexAttribute attribute>
        auto getBasicAttributes() noexcept { return this->_getAttributeSpan<attribute, attribute_basic_t<attribute>>(); }
        template <VertexAttribute attribute>
        auto getBasicAttributes() const noexcept { return this->_getAttributeSpan<attribute, attribute_basic_t<attribute>>(); }
        template <VertexAttribute attribute>
        auto getAttributes() noexcept { return this->_getAttributeSpan<attribute, attribute_t<attribute>>(); }
        template <VertexAttribute attribute>
        auto getAttributes() const noexcept { return this->_getAttributeSpan<attribute, attribute_t<attribute>>(); }
        template <VertexAttribute attribute>
        attribute_t<attribute> & getAttribute(size_t index) noexcept { return this->getAttributes<attribute>()[index]; }
        template <VertexAttribute attribute>
        const attribute_t<attribute> & getAttribute(size_t index) const noexcept { return this->getAttributes<attribute>()[index]; }
        
        template <std::ranges::contiguous_range Range>
        requires std::integral<std::ranges::range_value_t<Range>>
        Self & setIndices(Range && indices) { m_indices.assign_range(std::forward<Range>(indices)); return *this; }
        const IndexList & getIndices() const noexcept { return m_indices; }
        Self & addFace(Face face) { m_faces.emplace_back(std::move(face)); return *this; }
        const FaceList & getFaces() const noexcept { return m_faces; }
        uint32_t getVertexCount() const noexcept { return m_vertex_count; }
        uint32_t getIndexCount() const noexcept { return static_cast<uint32_t>(m_indices.size()); }

        template <typename Mapping = enum_value_type_mapping_traits<VectorType>::type>
        BufferWriteSegments generateInterleavedVertexBufferSegments(VertexAttributeFlags enabled_flags) const noexcept
        {
            return generate_interleaved_segments<Mapping>(*this, enabled_flags);
        }
    private:
        void ensureAttributeExist(VertexAttribute attribute)
        {
            auto & attributes_bytes = m_attributes_map[enum_decode::get_index(attribute)];
            if (attributes_bytes.empty()) {
                attributes_bytes.resize(m_vertex_count * enum_decode::get_size_in_bytes(enum_decode::get_vector_type(attribute)));
            }
        }
        template <VertexAttribute attribute, typename T>
        auto _getAttributeSpan() noexcept
        {
            auto & attributes_bytes = m_attributes_map[enum_decode::get_index(attribute)];
            return std::span<T>(reinterpret_cast<T *>(attributes_bytes.data()), attributes_bytes.size() / sizeof(T));
        }
        template <VertexAttribute attribute, typename T>
        auto _getAttributeSpan() const noexcept
        {
            auto & attributes_bytes = m_attributes_map[enum_decode::get_index(attribute)];
            return std::span<const T>(reinterpret_cast<const T *>(attributes_bytes.data()), attributes_bytes.size() / sizeof(T));
        }
    private:
        uint32_t m_vertex_count = 0;
        AttributesMap m_attributes_map;
        IndexList m_indices;
        FaceList m_faces;
    };

    template <typename Mapping = enum_value_type_mapping_traits<VectorType>::type, std::ranges::forward_range GeometryRange>
    requires std::is_reference_v<std::ranges::range_reference_t<GeometryRange>> and
        std::is_same_v<typename std::ranges::iterator_t<GeometryRange>::value_type, Geometry>
    BufferWriteSegments generate_interleaved_segments(GeometryRange && geometry_range, VertexAttributeFlags enabled_flags) noexcept
    {
        auto attributes = enum_values_v<VertexAttribute> | std::views::filter([enabled_flags](auto attribute) {
            return contains_flags(enabled_flags, enum_decode::to_flag_bit(attribute));
        });
        StructureLayout layout;
        for (auto attribute : attributes) {
            switch (enum_decode::get_vector_type(attribute)) {
                case VectorType::e2Float32: { layout.addField<enum_value_t<VectorType::e2Float32, Mapping>>(); } break;
                case VectorType::e3Float32: { layout.addField<enum_value_t<VectorType::e3Float32, Mapping>>(); } break;
                case VectorType::e4Float32: { layout.addField<enum_value_t<VectorType::e4Float32, Mapping>>(); } break;
                case VectorType::e4UInt32: { layout.addField<enum_value_t<VectorType::e4UInt32, Mapping>>(); } break;
                default: break;
            }
        }
        layout.create();
        BufferWriteSegments segments;
        size_t structural_size = layout.getStructualSize();
        size_t offset = 0;
        for (const auto & geometry : geometry_range) {
            size_t field_index = 0;
            for (auto attribute : attributes) {
                size_t type_size = enum_decode::get_size_in_bytes(enum_decode::get_vector_type(attribute));
                size_t src_offset = 0, dst_offset = layout.getFieldOffset(field_index++);
                auto attribute_bytes = geometry.getRawAttributes(attribute);
                if (attribute_bytes.empty()) { continue; }
                for (size_t i = 0; i < geometry.getVertexCount(); ++i) {
                    segments.add({attribute_bytes.subspan(src_offset, type_size), offset + dst_offset});
                    src_offset += type_size;
                    dst_offset += structural_size;
                }
            }
            offset += geometry.getVertexCount() * structural_size;
        }
        return segments;
    }

    template <typename Mapping = enum_value_type_mapping_traits<VectorType>::type>
    BufferWriteSegments generate_interleaved_segments(const Geometry & geometry, VertexAttributeFlags enabled_flags) noexcept
    {
        return generate_interleaved_segments<Mapping>(std::span(&geometry, 1), enabled_flags);
    }

    template <std::ranges::forward_range GeometryRange>
    requires std::is_reference_v<std::ranges::range_reference_t<GeometryRange>> and
        std::is_same_v<typename std::ranges::iterator_t<GeometryRange>::value_type, Geometry>
    Geometry::IndexList generate_merged_indices(GeometryRange && geometry_range) noexcept
    {
        Geometry::IndexList indices;
        size_t index_count = 0;
        for (const auto & geometry : geometry_range) { index_count += geometry.getIndexCount(); }
        indices.reserve(index_count);
        uint32_t vertex_offset = 0;
        for (const auto & geometry : geometry_range) { 
            indices.append_range(std::views::transform(geometry.getIndices(),
                [vertex_offset](uint32_t index) { return index + vertex_offset; }));
            vertex_offset += geometry.getVertexCount();
        }
        return indices;
    }
}


/* Example usage:
#include "Geometry.h"
#include "glsl_type_traits.h"
#include <iostream>

using namespace lcf;
namespace stdr = std::ranges;

int main() {
    constexpr uint32_t vertex_count = 3;
    
    std::vector<int> positions_i = {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1,
    };
    std::vector<float> positions = {
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
    };
    std::vector<Vector3D<float>> positions2 = {
        {0.5f, 0.5f, 0.5f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    };
    std::vector<Vector3D<float>> normals = {
        {2.0f, 2.0f, 2.0f},
        {2.0f, 2.0f, 2.0f},
        {2.0f, 2.0f, 2.0f},
    };
    std::vector<Vector2D<float>> texcoords = {
        {3.0f, 3.0f},
        {3.0f, 3.0f},
        {3.0f, 3.0f},
    };

    Geometry geometry {vertex_count};
    geometry.setAttributes<VertexAttribute::ePosition>(positions_i) //- works,  behaves like copy static_cast<float>(int), but not recommended
        .setAttributes<VertexAttribute::ePosition>(positions) //- works, type safe
        .setRawAttributes(VertexAttribute::ePosition, positions) //- works, behaves like memcpy, copy bytes
        .setAttributes<VertexAttribute::ePosition>(positions2) //- works, type safe
        .setAttribute<VertexAttribute::ePosition>(0, {1.0f, 1.0f, 1.0f})//- set single attribute
        .setAttributes<VertexAttribute::eNormal>(normals)
        .setAttributes<VertexAttribute::eTexCoord0>(texcoords);

    bool equal = stdr::equal(geometry.getAttributes<VertexAttribute::ePosition>(), positions2);
    
    std::cout << "is equal: " << equal << std::endl;

    std::vector<float> interleaved_data(vertex_count * 12);
    auto segments = geometry.generateInterleavedSegments();
    for (const auto & segment : segments) {
        memcpy(interleaved_data.data() + segment.getBeginOffsetInBytes() / sizeof(float), segment.getData(), segment.getSizeInBytes());
    }
    for (float value : interleaved_data) { std::cout << value << " "; } std::cout << std::endl;

    std::ranges::fill(interleaved_data, 0.0f);
    segments = geometry.generateInterleavedSegments<glsl::std140::enum_value_type_mapping_t>();
    for (const auto & segment : segments) {
        memcpy(interleaved_data.data() + segment.getBeginOffsetInBytes() / sizeof(float), segment.getData(), segment.getSizeInBytes());
    }
    for (float value : interleaved_data) { std::cout << value << " "; } std::cout << std::endl;

    std::ranges::fill(interleaved_data, 0.0f);
    segments = geometry.generateInterleavedSegments<glsl::std430::enum_value_type_mapping_t>();
    for (const auto & segment : segments) {
        memcpy(interleaved_data.data() + segment.getBeginOffsetInBytes() / sizeof(float), segment.getData(), segment.getSizeInBytes());
    }
    for (float value : interleaved_data) { std::cout << value << " "; } std::cout << std::endl;

    return 0;
}
*/
