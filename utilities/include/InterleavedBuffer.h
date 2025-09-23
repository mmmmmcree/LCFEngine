#pragma once

#include "StrideIterator.h"
#include "concepts/alignment_concept.h"
#include <vector>
#include <span>
#include <ranges>
#include <boost/align.hpp>


namespace lcf {
    class InterleavedBuffer
    {
        using Self = InterleavedBuffer;
    public:
        InterleavedBuffer() = default;
        bool isCreated() const noexcept { return not m_data.empty(); }
        size_t getStrideInBytes() const noexcept { return this->isCreated() ? m_offsets.back() : 1; }
        size_t getSizeInBytes() const noexcept { return m_data.size(); }
        size_t getSize() const noexcept { return m_data.size() / this->getStrideInBytes(); }
        std::byte * getData() noexcept { return m_data.data(); }
        const std::byte * getData() const noexcept { return m_data.data(); }
        std::span<std::byte> getDataSpan() noexcept { return m_data; }
        std::span<const std::byte> getDataSpan() const noexcept { return m_data; }
        Self & setStructuralAlignment(size_t alignment) noexcept
        {
            m_structural_alignment = std::max(m_structural_alignment, std::bit_ceil(alignment));
            return *this; 
        }
        Self & addField(size_t size_in_bytes, size_t alignment)
        {
            if (this->isCreated()) { return *this; }
            alignment = std::bit_ceil(alignment);
            m_structural_alignment = std::max(m_structural_alignment, alignment);
            m_offsets.emplace_back(alignment);
            m_offsets.emplace_back(size_in_bytes);
            return *this;
        }
        template <alignment_c TypeAlignmentInfo>
        Self & addField() { return this->addField(TypeAlignmentInfo::type_size, TypeAlignmentInfo::alignment); }
        void create(size_t size)
        {
            if (this->isCreated()) { return; }
            size_t n = m_offsets.size() / 2;
            std::vector<size_t> offsets(1, 0);
            offsets.reserve(n + 1);
            for (int i = 1; i < n; ++i) {
                size_t cur_alignment = m_offsets[i * 2];
                size_t prev_size = m_offsets[i * 2 - 1];
                offsets.emplace_back(boost::alignment::align_up(offsets.back() + prev_size, cur_alignment));
            }
            size_t structural_size = boost::alignment::align_up(offsets.back() + m_offsets.back(), m_structural_alignment);
            offsets.emplace_back(structural_size);
            m_offsets.swap(offsets);
            m_data.resize(size * structural_size);
        }
        void resize(size_t size)
        {
            if (this->isCreated()) { m_data.resize(size * this->getStrideInBytes()); }
            else { this->create(size); }
        }
        template <typename T>
        T & viewAt(size_t field_index, size_t index)
        {
            size_t offset = m_offsets[field_index] + index * this->getStrideInBytes();
            return *reinterpret_cast<T *>(m_data.data() + offset);
        }
        template <typename T>
        const T & viewAt(size_t field_index, size_t index) const
        {
            size_t offset = m_offsets[field_index] + index * this->getStrideInBytes();
            return *reinterpret_cast<const T *>(m_data.data() + offset);
        }
        template <typename T>
        auto view(size_t field_index) noexcept { return this->_view<T, false>(field_index); }
        template <typename T>
        auto view(size_t field_index) const noexcept { return this->_view<T, true>(field_index); }
        template <typename T>
        Self & setData(size_t field_index, size_t data_index, const T & data) noexcept
        {
            if (data_index >= this->getSize()) { return *this; }
            this->viewAt<T>(field_index, data_index) = data;
            return *this;
        }
        template <std::ranges::range Range>
        Self & setData(size_t field_index, Range data, size_t start_data_index = 0) noexcept
        {
            auto dst_it = this->view<std::ranges::range_value_t<Range>>(field_index).begin() + start_data_index;
            std::ranges::copy(data | std::views::take(this->getSize() - start_data_index), dst_it);
            return *this;
        }
    private:
        template <typename T, bool is_const>
        auto _view(size_t field_index) const noexcept
        {
            using Iterator = std::conditional_t<is_const, lcf::ConstStrideIterator<T>, lcf::StrideIterator<T>>;
            if (field_index >= m_offsets.size() - 1 or not this->isCreated())
            { return std::ranges::subrange<Iterator>{}; }
            Iterator begin(const_cast<std::byte *>(m_data.data()) + m_offsets[field_index], this->getStrideInBytes());
            return std::ranges::subrange(begin, begin + this->getSize());
        }
    private:
        size_t m_structural_alignment = 1u;
        std::vector<std::byte> m_data;
        std::vector<size_t> m_offsets;
    };
}