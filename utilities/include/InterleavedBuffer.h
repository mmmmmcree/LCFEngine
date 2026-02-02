#pragma once

#include "StructureLayout.h"
#include "type_traits/lcf_type_traits.h"
#include "log.h"

namespace lcf::impl {
    template <structure_layout_c StructureLayout>
    class InterleavedBufferIMPL
    {
        using Self = InterleavedBufferIMPL;
    public:
        InterleavedBufferIMPL() = default;
        bool isCreated() const noexcept { return not m_data.empty(); }
        size_t getStrideInBytes() const noexcept { return m_layout.getStructualSize(); }
        size_t getSizeInBytes() const noexcept { return m_data.size(); }
        size_t getSize() const noexcept { return m_data.size() / this->getStrideInBytes(); }
        std::span<const std::byte> getDataSpan() const noexcept { return m_data; }
        const StructureLayout & getLayout() const noexcept { return m_layout; }
        Self & addField(size_t size_in_bytes, size_t alignment)
        {
            if (this->isCreated()) { return *this; }
            m_layout.addField(size_in_bytes, alignment);
            return *this;
        }
        template <typename T>
        Self & addField() { return this->addField(size_of_v<T>, alignment_of_v<T>); }
        void create(size_t size)
        {
            if (this->isCreated()) { return; }
            m_layout.create();
            m_data.resize(size * this->getStrideInBytes());
        }
        void resize(size_t size)
        {
            if (this->isCreated()) { m_data.resize(size * this->getStrideInBytes()); }
            else { this->create(size); }
        }
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
        template <std::ranges::range Range>
        Self & setData(size_t field_index, Range && data, size_t start_data_index = 0) noexcept
        {
            using ValueType = std::ranges::range_value_t<Range>;
            if (m_layout.getFieldAlignedSize(field_index) < size_of_v<ValueType>) {
                std::runtime_error error("data size is too large for the field");
                lcf_log_error(error.what());
                throw error;
            }
            auto dst_it = this->view<ValueType>(field_index).begin() + start_data_index;
            std::ranges::copy(data | std::views::take(this->getSize() - start_data_index), dst_it);
            return *this;
        }
    private:
        StructureLayout m_layout;
        std::vector<std::byte> m_data;
    };
}

namespace lcf {
    using InterleavedBuffer = impl::InterleavedBufferIMPL<StructureLayout>;
}