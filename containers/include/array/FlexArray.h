#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <ranges>
#include <span>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

namespace lcf::detail {

    template <typename T>
    inline void migrate_trivial(T * dst_p, T * src_p, std::size_t count, std::size_t & constructed_count) noexcept
    {
        std::memcpy(static_cast<void *>(dst_p), static_cast<const void *>(src_p), count * sizeof(T));
        constructed_count = count;
    }

    template <typename T>
    inline void migrate_nothrow_move(T * dst_p, T * src_p, std::size_t count, std::size_t & constructed_count) noexcept
    {
        for (std::size_t i = 0; i < count; ++i) {
            ::new (static_cast<void *>(dst_p + i)) T(std::move(src_p[i]));
            constructed_count = i + 1;
        }
    }

    template <typename T>
    inline void migrate_checked(T * dst_p, T * src_p, std::size_t count, std::size_t & constructed_count)
    {
        constexpr bool k_prefer_move = not std::is_copy_constructible_v<T>;
        for (std::size_t i = 0; i < count; ++i) {
            if constexpr (k_prefer_move) {
                ::new (static_cast<void *>(dst_p + i)) T(std::move(src_p[i]));
            } else {
                ::new (static_cast<void *>(dst_p + i)) T(src_p[i]);
            }
            constructed_count = i + 1;
        }
    }

} // namespace lcf::detail

namespace lcf {

    template <typename T, std::unsigned_integral SizeType = std::uint32_t, typename Allocator = std::allocator<std::byte>>
    class FlexArray
    {
        struct Header { SizeType size; SizeType capacity; };
        static constexpr std::size_t k_data_offset = ((sizeof(Header) + alignof(T) - 1) / alignof(T)) * alignof(T);
        struct BlockGuard
        {
            BlockGuard(FlexArray * owner, T * data, std::size_t cap) noexcept : owner_p(owner), data_p(data), capacity_count(cap) {}
            BlockGuard(const BlockGuard &) = delete;
            BlockGuard & operator=(const BlockGuard &) = delete;
            ~BlockGuard() { if (data_p) { owner_p->destroy_prefix_and_release(data_p, constructed_count, capacity_count); } }
            T * release() noexcept { T * released_p = data_p; data_p = nullptr; return released_p; }
            FlexArray * owner_p = nullptr;
            T * data_p = nullptr;
            std::size_t capacity_count = 0;
            std::size_t constructed_count = 0;
        };
        using ByteAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;
        using ByteAllocTraits = std::allocator_traits<ByteAlloc>;
    public:
        using value_type = T;
        using size_type = std::size_t;
        using header_size_type = SizeType;
        using allocator_type = Allocator;
        using iterator = T *;
        using const_iterator = const T *;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    public:
        FlexArray() noexcept(std::is_nothrow_default_constructible_v<Allocator>) = default;
        explicit FlexArray(const Allocator & alloc) noexcept : m_allocator(alloc) {}
        FlexArray(std::size_t count, const T & value, const Allocator & alloc = {});
        explicit FlexArray(std::size_t count, const Allocator & alloc = {});
        template <std::input_iterator It>
        FlexArray(It first, It last, const Allocator & alloc = {});
        template <std::ranges::input_range R>
        FlexArray(std::from_range_t, R && r, const Allocator & alloc = {});
        FlexArray(std::initializer_list<T> initial_list, const Allocator & alloc = {}) : FlexArray(initial_list.begin(), initial_list.end(), alloc) {}
        FlexArray(const FlexArray & other) : FlexArray(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.m_allocator)) {}
        FlexArray(const FlexArray & other, const Allocator & alloc);
        FlexArray(FlexArray && other) noexcept : m_first(other.m_first), m_last(other.m_last), m_end(other.m_end), m_allocator(std::move(other.m_allocator))
        { other.resetPointers(); }
        FlexArray(FlexArray && other, const Allocator & alloc);
        ~FlexArray() { this->discard_block(); }
        FlexArray & operator=(const FlexArray & other);
        FlexArray & operator=(FlexArray && other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value or std::allocator_traits<Allocator>::is_always_equal::value);
        FlexArray & operator=(std::initializer_list<T> initial_list) { this->assign(initial_list.begin(), initial_list.end()); return *this; }
    public:
        std::size_t size() const noexcept { return static_cast<std::size_t>(m_last - m_first); }
        std::size_t capacity() const noexcept { return static_cast<std::size_t>(m_end - m_first); }
        bool isEmpty() const noexcept { return m_last == m_first; }
        void reserve(std::size_t n) { if (n > this->capacity()) { this->reallocateTo(n); } }
        void resize(std::size_t n) { this->resize(n, T{}); }
        void resize(std::size_t n, const T & value);
        void shrinkToFit();
        void clearAndShrink() noexcept { this->discard_block(); }
        [[nodiscard]] std::expected<void, std::error_code> tryReserve(std::size_t n) noexcept { return n <= this->capacity() ? std::expected<void, std::error_code>{} : this->tryReallocateTo(n); }
        T & operator[](std::size_t i) noexcept { return m_first[i]; }
        const T & operator[](std::size_t i) const noexcept { return m_first[i]; }
        T & at(std::size_t i);
        const T & at(std::size_t i) const;
        T & front() noexcept { return *m_first; }
        const T & front() const noexcept { return *m_first; }
        T & back() noexcept { return *(m_last - 1); }
        const T & back() const noexcept { return *(m_last - 1); }
        std::span<T> getDataSpan() noexcept { return {m_first, this->size()}; }
        std::span<const T> getDataSpan() const noexcept { return {m_first, this->size()}; }
        void pushBack(const T & v)
        {
            if (m_last == m_end) [[unlikely]] { this->ensure_one_more_capacity(); }
            ::new (static_cast<void *>(m_last)) T(v);
            ++m_last;
        }
        void pushBack(T && v)
        {
            if (m_last == m_end) [[unlikely]] { this->ensure_one_more_capacity(); }
            ::new (static_cast<void *>(m_last)) T(std::move(v));
            ++m_last;
        }
        template <typename... Args>
        T & emplaceBack(Args &&... args);
        template <typename... Args>
        [[nodiscard]] std::expected<std::reference_wrapper<T>, std::error_code> tryEmplaceBack(Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args &&...>);
        void popBack() noexcept;
        iterator insert(const_iterator pos, const T & v) { return this->emplace(pos, v); }
        iterator insert(const_iterator pos, T && v) { return this->emplace(pos, std::move(v)); }
        template <typename... Args>
        iterator emplace(const_iterator pos, Args &&... args);
        iterator erase(const_iterator pos) { return this->erase(pos, pos + 1); }
        iterator erase(const_iterator first, const_iterator last);
        void clear() noexcept { this->destroyAllElements(); m_last = m_first; }
        void assign(std::size_t count, const T & value);
        template <std::input_iterator It>
        void assign(It first, It last);
        void assign(std::initializer_list<T> initial_list) { this->assign(initial_list.begin(), initial_list.end()); }
        template <std::ranges::input_range R>
        void assignRange(R && r) { this->clear(); this->appendRange(std::forward<R>(r)); }
        template <std::ranges::input_range R>
        void appendRange(R && r);
        template <std::ranges::input_range R>
        iterator insertRange(const_iterator pos, R && r);
        std::span<const std::byte> getCountedBytes() const noexcept { return this->compute_byte_span(this->size()); }
        std::span<const std::byte> getCountedBytesWithCapacity() const noexcept { return this->compute_byte_span(this->capacity()); }
        iterator begin() noexcept { return m_first; }
        const_iterator begin() const noexcept { return m_first; }
        iterator end() noexcept { return m_last; }
        const_iterator end() const noexcept { return m_last; }
        const_iterator cbegin() const noexcept { return m_first; }
        const_iterator cend() const noexcept { return m_last; }
        reverse_iterator rbegin() noexcept { return reverse_iterator(m_last); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(m_last); }
        reverse_iterator rend() noexcept { return reverse_iterator(m_first); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(m_first); }
        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(m_last); }
        const_reverse_iterator crend() const noexcept { return const_reverse_iterator(m_first); }
        allocator_type get_allocator() const noexcept { return m_allocator; }
    private:
        [[nodiscard]] std::expected<T *, std::error_code> tryAcquireBlock(std::size_t capacity_count) noexcept;
        T * acquireBlock(std::size_t capacity_count);
        void releaseBlock(T * data_p, std::size_t capacity_count) noexcept;
        void destroyAllElements() noexcept;
        void destroy_prefix_and_release(T * data_p, std::size_t constructed_count, std::size_t capacity_count) noexcept;
        [[nodiscard]] std::expected<void, std::error_code> tryReallocateTo(std::size_t new_capacity) noexcept;
        void reallocateTo(std::size_t new_capacity);
        void discard_block() noexcept;
        void ensure_one_more_capacity();
        [[nodiscard]] std::expected<void, std::error_code> try_ensure_one_more_capacity() noexcept;
        std::span<const std::byte> compute_byte_span(std::size_t element_count) const noexcept;
        template <typename Factory>
        void build_filled_block(std::size_t count, Factory && factory);
        void setPointers(T * data_p, std::size_t size_count, std::size_t capacity_count) noexcept
        {
            m_first = data_p;
            m_last = data_p + size_count;
            m_end = data_p + capacity_count;
        }
        void resetPointers() noexcept
        {
            m_first = nullptr;
            m_last = nullptr;
            m_end = nullptr;
        }
        void stealPointersFrom(FlexArray & other) noexcept
        {
            m_first = other.m_first;
            m_last = other.m_last;
            m_end = other.m_end;
            other.resetPointers();
        }
    private:
        static std::size_t get_block_bytes_for(std::size_t capacity_count) noexcept { return k_data_offset + capacity_count * sizeof(T); }
        static Header * header_of(T * data_p) noexcept { return reinterpret_cast<Header *>(reinterpret_cast<std::byte *>(data_p) - k_data_offset); }
        static const Header * header_of(const T * data_p) noexcept { return reinterpret_cast<const Header *>(reinterpret_cast<const std::byte *>(data_p) - k_data_offset); }
        static std::size_t get_grow_count(std::size_t current_capacity, std::size_t requested_capacity);
    private:
        T * m_first = nullptr;
        T * m_last = nullptr;
        T * m_end = nullptr;
        Allocator m_allocator;
    };

} // namespace lcf

namespace lcf {

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    FlexArray<T, SizeType, Allocator>::FlexArray(std::size_t count, const T & value, const Allocator & alloc) : m_allocator(alloc)
    {
        this->build_filled_block(count, [&](std::size_t) -> decltype(auto) { return (value); });
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    FlexArray<T, SizeType, Allocator>::FlexArray(std::size_t count, const Allocator & alloc) : m_allocator(alloc)
    {
        this->build_filled_block(count, [](std::size_t) -> decltype(auto) { return T{}; });
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::input_iterator It>
    FlexArray<T, SizeType, Allocator>::FlexArray(It first, It last, const Allocator & alloc) : m_allocator(alloc)
    {
        if constexpr (std::forward_iterator<It>) {
            const std::size_t count = static_cast<std::size_t>(std::distance(first, last));
            auto it = first;
            this->build_filled_block(count, [&](std::size_t) -> decltype(auto) { return *it++; });
        } else {
            for (auto it = first; it != last; ++it) { this->emplaceBack(*it); }
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::ranges::input_range R>
    FlexArray<T, SizeType, Allocator>::FlexArray(std::from_range_t, R && r, const Allocator & alloc) : m_allocator(alloc)
    {
        if constexpr (std::ranges::sized_range<R>) {
            const std::size_t count = static_cast<std::size_t>(std::ranges::size(r));
            auto it = std::ranges::begin(r);
            this->build_filled_block(count, [&](std::size_t) -> decltype(auto) { return *it++; });
        } else {
            for (auto && element : r) { this->emplaceBack(std::forward<decltype(element)>(element)); }
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    FlexArray<T, SizeType, Allocator>::FlexArray(const FlexArray & other, const Allocator & alloc) : m_allocator(alloc)
    {
        const std::size_t count = other.size();
        if (count == 0) { return; }
        if constexpr (std::is_trivially_copyable_v<T>) {
            T * new_data_p = this->acquireBlock(count);
            std::memcpy(static_cast<void *>(new_data_p), static_cast<const void *>(other.m_first), count * sizeof(T));
            this->setPointers(new_data_p, count, count);
        } else {
            const T * src_p = other.m_first;
            this->build_filled_block(count, [&](std::size_t i) -> decltype(auto) { return src_p[i]; });
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    FlexArray<T, SizeType, Allocator>::FlexArray(FlexArray && other, const Allocator & alloc) : m_allocator(alloc)
    {
        if (m_allocator == other.m_allocator) {
            this->stealPointersFrom(other);
            return;
        }
        T * src_p = other.m_first;
        this->build_filled_block(other.size(), [&](std::size_t i) -> decltype(auto) { return std::move(src_p[i]); });
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::operator=(const FlexArray & other) -> FlexArray &
    {
        if (this == &other) { return *this; }
        if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
            if (m_allocator != other.m_allocator) { this->clearAndShrink(); }
        }
        this->assign(other.begin(), other.end());
        return *this;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::operator=(FlexArray && other)
        noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value or std::allocator_traits<Allocator>::is_always_equal::value)
        -> FlexArray &
    {
        if (this == &other) { return *this; }
        constexpr bool k_propagate = std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value;
        constexpr bool k_always_equal = std::allocator_traits<Allocator>::is_always_equal::value;

        if constexpr (k_propagate or k_always_equal) {
            this->discard_block();
            this->stealPointersFrom(other);
            if constexpr (k_propagate) { m_allocator = std::move(other.m_allocator); }
            return *this;
        }
        if (m_allocator == other.m_allocator) {
            this->discard_block();
            this->stealPointersFrom(other);
            return *this;
        }
        this->clear();
        this->reserve(other.size());
        T * src_p = other.m_first;
        const std::size_t count = other.size();
        for (std::size_t i = 0; i < count; ++i) {
            ::new (static_cast<void *>(m_last)) T(std::move(src_p[i]));
            ++m_last;
        }
        return *this;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::resize(std::size_t n, const T & value)
    {
        const std::size_t current_size = this->size();
        if (n == current_size) { return; }
        if (n < current_size) {
            for (T * p = m_first + n; p != m_last; ++p) { std::destroy_at(p); }
            m_last = m_first + n;
            return;
        }
        if (n > this->capacity()) { this->reallocateTo(get_grow_count(this->capacity(), n)); }
        const T * stop_p = m_first + n;
        while (m_last != stop_p) {
            ::new (static_cast<void *>(m_last)) T(value);
            ++m_last;
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::shrinkToFit()
    {
        const std::size_t current_size = this->size();
        if (current_size == this->capacity()) { return; }
        if (current_size == 0) { this->clearAndShrink(); return; }
        this->reallocateTo(current_size);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    T & FlexArray<T, SizeType, Allocator>::at(std::size_t i)
    {
        if (i >= this->size()) { throw std::out_of_range("lcf::FlexArray::at index out of range"); }
        return m_first[i];
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    const T & FlexArray<T, SizeType, Allocator>::at(std::size_t i) const
    {
        if (i >= this->size()) { throw std::out_of_range("lcf::FlexArray::at index out of range"); }
        return m_first[i];
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <typename... Args>
    [[gnu::always_inline]] inline T & FlexArray<T, SizeType, Allocator>::emplaceBack(Args &&... args)
    {
        if (m_last == m_end) [[unlikely]] { this->ensure_one_more_capacity(); }
        T * slot_p = m_last;
        ::new (static_cast<void *>(slot_p)) T(std::forward<Args>(args)...);
        m_last = slot_p + 1;
        return *slot_p;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <typename... Args>
    auto FlexArray<T, SizeType, Allocator>::tryEmplaceBack(Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args &&...>)
        -> std::expected<std::reference_wrapper<T>, std::error_code>
    {
        if (m_last == m_end) {
            auto reserved = this->try_ensure_one_more_capacity();
            if (not reserved.has_value()) { return std::unexpected{reserved.error()}; }
        }
        if constexpr (std::is_nothrow_constructible_v<T, Args &&...>) {
            ::new (static_cast<void *>(m_last)) T(std::forward<Args>(args)...);
        } else {
            try {
                ::new (static_cast<void *>(m_last)) T(std::forward<Args>(args)...);
            } catch (...) {
                return std::unexpected{std::make_error_code(std::errc::operation_canceled)};
            }
        }
        T & ref = *m_last;
        ++m_last;
        return std::ref(ref);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::popBack() noexcept
    {
        if (m_last == m_first) { return; }
        --m_last;
        std::destroy_at(m_last);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <typename... Args>
    auto FlexArray<T, SizeType, Allocator>::emplace(const_iterator pos, Args &&... args) -> iterator
    {
        const std::size_t index = static_cast<std::size_t>(pos - m_first);
        if (m_last == m_end) { this->ensure_one_more_capacity(); }
        T * data_p = m_first;
        const std::size_t old_size = this->size();
        if (index < old_size) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(static_cast<void *>(data_p + index + 1), static_cast<const void *>(data_p + index), (old_size - index) * sizeof(T));
                ::new (static_cast<void *>(data_p + index)) T(std::forward<Args>(args)...);
            } else {
                ::new (static_cast<void *>(data_p + old_size)) T(std::move(data_p[old_size - 1]));
                for (std::size_t i = old_size - 1; i > index; --i) { data_p[i] = std::move(data_p[i - 1]); }
                data_p[index] = T(std::forward<Args>(args)...);
            }
        } else {
            ::new (static_cast<void *>(m_last)) T(std::forward<Args>(args)...);
        }
        ++m_last;
        return data_p + index;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::erase(const_iterator first, const_iterator last) -> iterator
    {
        T * data_p = m_first;
        const std::size_t first_index = static_cast<std::size_t>(first - data_p);
        const std::size_t last_index = static_cast<std::size_t>(last - data_p);
        const std::size_t old_size = this->size();
        const std::size_t erase_count = last_index - first_index;
        if (erase_count == 0) { return data_p + first_index; }
        if constexpr (std::is_trivially_copyable_v<T>) {
            const std::size_t tail_count = old_size - last_index;
            std::memmove(static_cast<void *>(data_p + first_index), static_cast<const void *>(data_p + last_index), tail_count * sizeof(T));
        } else {
            for (std::size_t i = last_index; i < old_size; ++i) { data_p[i - erase_count] = std::move(data_p[i]); }
            for (std::size_t i = old_size - erase_count; i < old_size; ++i) { std::destroy_at(data_p + i); }
        }
        m_last -= erase_count;
        return data_p + first_index;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::assign(std::size_t count, const T & value)
    {
        this->clear();
        if (count > this->capacity()) { this->reallocateTo(count); }
        for (std::size_t i = 0; i < count; ++i) {
            ::new (static_cast<void *>(m_last)) T(value);
            ++m_last;
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::input_iterator It>
    void FlexArray<T, SizeType, Allocator>::assign(It first, It last)
    {
        this->clear();
        if constexpr (std::forward_iterator<It>) {
            const std::size_t count = static_cast<std::size_t>(std::distance(first, last));
            if (count > this->capacity()) { this->reallocateTo(count); }
            for (auto it = first; it != last; ++it) {
                ::new (static_cast<void *>(m_last)) T(*it);
                ++m_last;
            }
        } else {
            for (auto it = first; it != last; ++it) { this->emplaceBack(*it); }
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::ranges::input_range R>
    void FlexArray<T, SizeType, Allocator>::appendRange(R && r)
    {
        if constexpr (std::ranges::sized_range<R>) {
            const std::size_t add_count = static_cast<std::size_t>(std::ranges::size(r));
            this->reserve(this->size() + add_count);
        }
        for (auto && element : r) { this->emplaceBack(std::forward<decltype(element)>(element)); }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::ranges::input_range R>
    auto FlexArray<T, SizeType, Allocator>::insertRange(const_iterator pos, R && r) -> iterator
    {
        const std::size_t insert_index = static_cast<std::size_t>(pos - m_first);
        if constexpr (std::ranges::sized_range<R>) {
            const std::size_t add_count = static_cast<std::size_t>(std::ranges::size(r));
            this->reserve(this->size() + add_count);
        }
        std::size_t current_index = insert_index;
        for (auto && element : r) {
            this->emplace(m_first + current_index, std::forward<decltype(element)>(element));
            ++current_index;
        }
        return m_first + insert_index;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::tryAcquireBlock(std::size_t capacity_count) noexcept -> std::expected<T *, std::error_code>
    {
        constexpr std::size_t k_size_type_max = static_cast<std::size_t>(std::numeric_limits<SizeType>::max());
        if (capacity_count > k_size_type_max) { return std::unexpected{std::make_error_code(std::errc::value_too_large)}; }
        const std::size_t bytes = get_block_bytes_for(capacity_count);
        ByteAlloc byte_alloc(m_allocator);
        std::byte * raw_p = nullptr;
        try {
            raw_p = ByteAllocTraits::allocate(byte_alloc, bytes);
        } catch (...) {
            return std::unexpected{std::make_error_code(std::errc::not_enough_memory)};
        }
        ::new (static_cast<void *>(raw_p)) Header{static_cast<SizeType>(0), static_cast<SizeType>(capacity_count)};
        return std::launder(reinterpret_cast<T *>(raw_p + k_data_offset));
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::acquireBlock(std::size_t capacity_count) -> T *
    {
        auto result = this->tryAcquireBlock(capacity_count);
        if (not result.has_value()) { throw std::system_error{result.error()}; }
        return *result;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::releaseBlock(T * data_p, std::size_t capacity_count) noexcept
    {
        if (not data_p) { return; }
        Header * header_p = header_of(data_p);
        const std::size_t bytes = get_block_bytes_for(capacity_count);
        std::destroy_at(header_p);
        ByteAlloc byte_alloc(m_allocator);
        ByteAllocTraits::deallocate(byte_alloc, reinterpret_cast<std::byte *>(header_p), bytes);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::destroyAllElements() noexcept
    {
        for (T * p = m_first; p != m_last; ++p) {
            std::destroy_at(p);
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::destroy_prefix_and_release(T * data_p, std::size_t constructed_count, std::size_t capacity_count) noexcept
    {
        if (not data_p) { return; }
        for (std::size_t i = 0; i < constructed_count; ++i) {
            std::destroy_at(data_p + i);
        }
        this->releaseBlock(data_p, capacity_count);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::tryReallocateTo(std::size_t new_capacity) noexcept -> std::expected<void, std::error_code>
    {
        const std::size_t current_size = this->size();
        if (new_capacity < current_size) { new_capacity = current_size; }

        auto block_result = this->tryAcquireBlock(new_capacity);
        if (not block_result.has_value()) { return std::unexpected{block_result.error()}; }

        BlockGuard guard(this, *block_result, new_capacity);
        T * new_data_p = guard.data_p;
        T * old_data_p = m_first;

        if (old_data_p and current_size > 0) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                lcf::detail::migrate_trivial<T>(new_data_p, old_data_p, current_size, guard.constructed_count);
            } else if constexpr (std::is_nothrow_move_constructible_v<T>) {
                lcf::detail::migrate_nothrow_move<T>(new_data_p, old_data_p, current_size, guard.constructed_count);
            } else {
                try {
                    lcf::detail::migrate_checked<T>(new_data_p, old_data_p, current_size, guard.constructed_count);
                } catch (...) {
                    return std::unexpected{std::make_error_code(std::errc::operation_canceled)};
                }
            }
        }

        T * accepted_p = guard.release();

        const std::size_t old_capacity = this->capacity();
        this->destroyAllElements();
        this->releaseBlock(m_first, old_capacity);

        this->setPointers(accepted_p, current_size, new_capacity);
        return {};
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::reallocateTo(std::size_t new_capacity)
    {
        auto result = this->tryReallocateTo(new_capacity);
        if (not result.has_value()) { throw std::system_error{result.error()}; }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::discard_block() noexcept
    {
        if (not m_first) { return; }
        const std::size_t cap = this->capacity();
        this->destroyAllElements();
        this->releaseBlock(m_first, cap);
        this->resetPointers();
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::ensure_one_more_capacity()
    {
        if (m_last == m_end) {
            const std::size_t cap = this->capacity();
            this->reallocateTo(get_grow_count(cap, cap + 1));
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::try_ensure_one_more_capacity() noexcept -> std::expected<void, std::error_code>
    {
        if (m_last != m_end) { return {}; }
        const std::size_t cap = this->capacity();
        return this->tryReallocateTo(get_grow_count(cap, cap + 1));
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    std::span<const std::byte> FlexArray<T, SizeType, Allocator>::compute_byte_span(std::size_t element_count) const noexcept
    {
        if (not m_first) { return {}; }
        Header * header_p = header_of(m_first);
        header_p->size = static_cast<SizeType>(m_last - m_first);
        const auto * raw_p = reinterpret_cast<const std::byte *>(header_p);
        const std::size_t bytes = sizeof(Header) + element_count * sizeof(T);
        return {raw_p, bytes};
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <typename Factory>
    void FlexArray<T, SizeType, Allocator>::build_filled_block(std::size_t count, Factory && factory)
    {
        if (count == 0) { return; }
        T * new_data_p = this->acquireBlock(count);
        BlockGuard guard(this, new_data_p, count);
        for (std::size_t i = 0; i < count; ++i) {
            ::new (static_cast<void *>(new_data_p + i)) T(factory(i));
            guard.constructed_count = i + 1;
        }
        this->setPointers(guard.release(), count, count);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    std::size_t FlexArray<T, SizeType, Allocator>::get_grow_count(std::size_t current_capacity, std::size_t requested_capacity)
    {
        constexpr std::size_t k_min_capacity = 4;
        std::size_t grown = current_capacity + current_capacity / 2;
        if (grown < k_min_capacity) { grown = k_min_capacity; }
        return grown > requested_capacity ? grown : requested_capacity;
    }

} // namespace lcf
