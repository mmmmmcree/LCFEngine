#pragma once

#include "StrideIterator.h"
#include "concepts/alignment_concept.h"
#include <vector>
#include <ranges>
#include <bit>
#include <boost/align.hpp>

namespace lcf {
    template <typename T>
    concept structure_layout_c = requires(T layout, size_t index) {
        { layout.getStructualSize() } -> std::convertible_to<size_t>;
        { layout.getFieldCount() } -> std::convertible_to<size_t>;
        { layout.getFieldOffset(index) } -> std::convertible_to<size_t>;
        { layout.getFieldAlignedSize(index) } -> std::convertible_to<size_t>;
    };

    class StructureLayout
    {
        using Self = StructureLayout;
    public:
        StructureLayout() = default;
        StructureLayout(const Self &) = default;
        StructureLayout(Self &&) = default;
        StructureLayout & operator=(const Self &) = default;
        StructureLayout & operator=(Self &&) = default;
        bool isCreated() const noexcept { return m_structural_alignment > 1u; }
        Self & addField(size_t size_in_bytes, size_t alignment)
        {
            if (this->isCreated()) { return *this; }
            alignment = std::bit_ceil(alignment);
            m_offsets.emplace_back(alignment);
            m_offsets.emplace_back(size_in_bytes);
            return *this;
        }
        template <alignment_c TypeAlignmentInfo>
        Self & addField() { return this->addField(TypeAlignmentInfo::type_size, TypeAlignmentInfo::alignment); }
        void create()
        {
            if (this->isCreated()) { return; }
            size_t n = m_offsets.size() / 2;
            std::vector<size_t> offsets(1, 0);
            offsets.reserve(n + 1);
            for (int i = 1; i < n; ++i) {
                size_t cur_alignment = m_offsets[i * 2];
                size_t prev_size = m_offsets[i * 2 - 1];
                offsets.emplace_back(boost::alignment::align_up(offsets.back() + prev_size, cur_alignment));
                m_structural_alignment = std::max(m_structural_alignment, cur_alignment);
            }
            size_t structural_size = boost::alignment::align_up(offsets.back() + m_offsets.back(), m_structural_alignment);
            offsets.emplace_back(structural_size);
            m_offsets.swap(offsets);
        }
        size_t getStructualSize() const noexcept { return this->isCreated() ? m_offsets.back() : 1; }
        size_t getStructualAlignment() const noexcept { return m_structural_alignment; }
        size_t getFieldCount() const noexcept { return m_offsets.size() - 1; }
        size_t getFieldOffset(size_t field_index) const { return m_offsets.at(field_index); }
        size_t getFieldAlignedSize(size_t field_index) const { return m_offsets.at(field_index + 1) - m_offsets[field_index]; }
    private:
        size_t m_structural_alignment = 1u;
        std::vector<size_t> m_offsets;
    };

    template <structure_layout_c StructureLayout, typename T, bool is_const>
    inline auto extract_field_at(const StructureLayout & layout, size_t field_index, size_t index, std::span<const std::byte> bytes) noexcept
    {
        size_t offset = layout.getFieldOffset(field_index) + index * layout.getStructualSize();
        if constexpr (is_const) {
            return reinterpret_cast<const T *>(bytes.data() + offset);
        } else {
            return reinterpret_cast<T *>(const_cast<std::byte *>(bytes.data()) + offset);
        }
    }

    template <structure_layout_c StructureLayout, typename T, bool is_const>
    inline auto extract_field_range(const StructureLayout & layout, size_t field_index, std::span<const std::byte> bytes) noexcept
    {
        using Iterator = std::conditional_t<is_const, lcf::ConstStrideIterator<T>, lcf::StrideIterator<T>>;
        if (field_index >= layout.getFieldCount())
        { return std::ranges::subrange<Iterator>{}; }
        Iterator begin(const_cast<std::byte *>(bytes.data()) + layout.getFieldOffset(field_index), layout.getStructualSize());
        Iterator end(const_cast<std::byte *>(bytes.data() + bytes.size()) + layout.getFieldOffset(field_index), layout.getStructualSize());
        return std::ranges::subrange(begin, end);
    }

    template <structure_layout_c StructureLayout, typename T>
    void set_field_at(const StructureLayout & layout, size_t field_index, size_t data_index, std::span<std::byte> bytes, T && data) noexcept
    {
        if (data_index >= bytes.size() / layout.getStructualSize()) { return; }
        *extract_field_at<StructureLayout, T, false>(layout, field_index, data_index, bytes) = data;
    }
}
