#pragma once

#include "StructureLayout.h"

namespace lcf::impl {
    template <structure_layout_c StructureLayout>
    class InterleavedSpanIMPL
    {
        using Self = InterleavedSpanIMPL;
    public:
        InterleavedSpanIMPL(const StructureLayout & layout, std::span<std::byte> data) : m_layout(layout), m_data(data) {}
        template <typename T>
        T & viewAt(size_t field_index, size_t index) { return *extract_field_at<StructureLayout, T, false>(m_layout, field_index, index, m_data); }
        template <typename T>
        const T & viewAt(size_t field_index, size_t index) const { return *extract_field_at<StructureLayout, T, true>(m_layout, field_index, index, m_data); }
        template <typename T>
        auto view(size_t field_index) noexcept { return extract_field_range<StructureLayout ,T, false>(m_layout, field_index, m_data); }
        template <typename T>
        auto view(size_t field_index) const noexcept { return extract_field_range<StructureLayout, T, true>(m_layout, field_index, m_data); }
        template <typename T>
        Self & setData(size_t field_index, size_t data_index, T && data) noexcept
        {
            if (data_index >= this->getSize()) { return *this; }
            this->viewAt<T>(field_index, data_index) = data;
            return *this;
        }
        template <std::ranges::range Span>
        Self & setData(size_t field_index, Span && data, size_t start_data_index = 0) noexcept
        {
            auto dst_it = this->view<std::ranges::range_value_t<Span>>(field_index).begin() + start_data_index;
            std::ranges::copy(data | std::views::take(this->getSize() - start_data_index), dst_it);
            return *this;
        }
    private:
        StructureLayout m_layout;
        std::span<std::byte> m_data;
    };

    template <structure_layout_c StructureLayout>
    class ConstInterleavedSpanIMPL
    {
        using Self = ConstInterleavedSpanIMPL;
    public:
        ConstInterleavedSpanIMPL(const StructureLayout & layout, std::span<const std::byte> data) : m_layout(layout), m_data(data) {}
        template <typename T>
        const T & viewAt(size_t field_index, size_t index) { return *extract_field_at<StructureLayout, T, true>(m_layout, field_index, index, m_data); }
        template <typename T>
        const T & viewAt(size_t field_index, size_t index) const { return *extract_field_at<StructureLayout, T, true>(m_layout, field_index, index, m_data); }
        template <typename T>
        auto view(size_t field_index) noexcept { return extract_field_range<StructureLayout ,T, true>(m_layout, field_index, m_data); }
        template <typename T>
        auto view(size_t field_index) const noexcept { return extract_field_range<StructureLayout, T, true>(m_layout, field_index, m_data); }
    private:
        StructureLayout m_layout;
        std::span<const std::byte> m_data;
    };
}

namespace lcf {
    using InterleavedSpan = impl::InterleavedSpanIMPL<StructureLayout>;
    using ConstInterleavedSpan = impl::ConstInterleavedSpanIMPL<StructureLayout>;
}