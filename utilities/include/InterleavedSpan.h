#pragma once

#include <vector>
#include <span>
#include <ranges>
#include "StrideIterator.h"

namespace lcf::impl {
    template <bool IsConst>
    class InterleavedSpanImpl
    {
        using Byte = std::conditional_t<IsConst, const std::byte, std::byte>;
        using ByteSpan = std::span<Byte>;
        template <typename T>
        using DataSpan = std::conditional_t<IsConst, std::span<const T>, std::span<T>>;
        template <typename T>
        using Iterator = std::conditional_t<IsConst, ConstStrideIterator<T>, StrideIterator<T>>;
        template <typename T>
        using ConstIterator = lcf::ConstStrideIterator<T>;
    public:
        InterleavedSpanImpl() = default;
        InterleavedSpanImpl(std::span<const size_t>attr_sizes_in_bytes) { this->setLayout(attr_sizes_in_bytes); }
        InterleavedSpanImpl(std::initializer_list<size_t> attr_sizes_in_bytes) : InterleavedSpanImpl(std::span<const size_t>(attr_sizes_in_bytes)) { }
        void setLayout(std::span<const size_t> attr_sizes_in_bytes)
        {
            m_offsets.resize(attr_sizes_in_bytes.size() + 1, 0u);
            size_t offset = 0;
            for (int i = 0; i < attr_sizes_in_bytes.size(); ++i) {
                m_offsets[i] = offset;
                offset += attr_sizes_in_bytes[i];
            }
            m_offsets.back() = offset;            
        }
        template <std::ranges::range Range>
        void bind(Range &&range)
        {
            if constexpr (IsConst) { m_data = std::as_bytes(std::span(range)); }
            else { m_data = std::as_writable_bytes(std::span(range)); }
        }
        template <typename T>
        auto view(size_t index)
        {
            if (index >= m_offsets.size() - 1) { return std::ranges::subrange(Iterator<T>(), Iterator<T>()); }
            Iterator<T> begin(m_data.data() + m_offsets[index], this->getStrideInBytes());
            Iterator<T> end(m_data.data() + m_offsets[index] + m_data.size(), this->getStrideInBytes());
            return std::ranges::subrange(begin, end);
        }
        template <typename T>
        auto view(size_t index) const
        {
            if (index >= m_offsets.size() - 1) { return std::ranges::subrange(ConstIterator<T>(), ConstIterator<T>()); }
            ConstIterator<T> begin(m_data.data() + m_offsets[index], this->getStrideInBytes());
            ConstIterator<T> end(m_data.data() + m_offsets[index] + m_data.size(), this->getStrideInBytes());
            return std::ranges::subrange(begin, end);
        }
        size_t getStrideInBytes() const { return m_offsets.back(); }
        Byte *getData() { return m_data.data(); }
        size_t getSizeInBytes() const { return m_data.size(); }
    private:
        ByteSpan m_data;
        std::vector<size_t> m_offsets;
    };
}

namespace lcf {
    class InterleavedSpan : public lcf::impl::InterleavedSpanImpl<false>
    {
        using Base = lcf::impl::InterleavedSpanImpl<false>;
    public:
        using Base::Base;
        template <typename T>
        void setData(size_t attr_index, size_t index, const T &value)
        {
            auto range = this->template view<T>(attr_index);
            if (index >= range.size()) { return; }
            range[index] = value;
        }
        template <typename T>
        void setData(size_t attr_index, std::span<const T> values, size_t offset = 0)
        {
            auto range = this->template view<T>(attr_index);
            if (offset >= range.size()) { return; }
            std::ranges::copy(values | std::views::take(range.size() - offset), range.begin() + offset);
        }
    };

    using ConstInterleavedSpan = lcf::impl::InterleavedSpanImpl<true>;
}