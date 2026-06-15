#pragma once

#include <iterator>

namespace lcf::impl {
    template <typename T>
    class StrideIteratorImpl
    {
        using BytePtr = std::conditional_t<std::is_const_v<T>, const std::byte *, std::byte *>;
        using VoidPtr = std::conditional_t<std::is_const_v<T>, const void *, void *>;
        using Self = StrideIteratorImpl<T>;
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;
        StrideIteratorImpl() = default;
        explicit StrideIteratorImpl(VoidPtr data, size_t stride_in_bytes) :
            m_data(static_cast<BytePtr>(data)), m_stride(stride_in_bytes) { }
        StrideIteratorImpl(const Self &other) { *this = other; }
        Self& operator=(const Self &other)
        {
            m_data = other.m_data;
            m_stride = other.m_stride;
            return *this;
        }
        reference operator*() const { return *reinterpret_cast<pointer>(m_data); }
        pointer operator->() const { return reinterpret_cast<const pointer>(m_data); }
        Self &operator++() { m_data += m_stride; return *this; }
        Self operator++(int) { Self tmp(*this); m_data += m_stride; return tmp; }
        Self &operator--() { m_data -= m_stride; return *this; }
        Self operator--(int) { Self tmp(*this); m_data -= m_stride; return tmp; }
        Self &operator+=(difference_type n) { m_data += n * m_stride; return *this; }
        Self &operator-=(difference_type n) { m_data -= n * m_stride; return *this; }
        Self operator+(difference_type n) const { Self tmp(*this); tmp += n; return tmp; }
        Self operator-(difference_type n) const { Self tmp(*this); tmp -= n; return tmp; }
        difference_type operator-(const Self &other) const { return (m_data - other.m_data) / m_stride; }
        reference operator[](difference_type n) const { return *(*this + n); }
        bool operator==(const Self &other) const { return m_data == other.m_data; }
        auto operator<=>(const Self &other) const { return m_data <=> other.m_data; }
        friend Self operator+(difference_type n, const Self& iter) { return iter + n; }
    private:
        BytePtr m_data = nullptr;
        size_t m_stride = 0;
    };
}

namespace lcf {
    template <typename T>
    using StrideIterator = impl::StrideIteratorImpl<T>;

    template <typename T>
    using ConstStrideIterator = impl::StrideIteratorImpl<const T>;
}