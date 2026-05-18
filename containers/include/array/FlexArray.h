#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <span>
#include <system_error>
#include <type_traits>
#include <memory>


namespace {

    inline constexpr std::size_t get_array_grow_count(std::size_t current_capacity, std::size_t requested_capacity) noexcept
    {
        constexpr std::size_t k_min_capacity = 4;
        std::size_t grown = current_capacity + (current_capacity >> 1);
        return std::max(std::max(grown, k_min_capacity), requested_capacity);
    }

    template <typename T>
    inline constexpr bool is_nothrow_move_v = std::is_nothrow_move_constructible_v<T>;

    template <typename T>
    inline constexpr bool is_nothrow_copy_v = std::is_nothrow_copy_constructible_v<T>;

    template <typename T>
    inline constexpr bool is_nothrow_relocate_v = std::is_nothrow_move_constructible_v<T> || (!std::is_move_constructible_v<T> && std::is_nothrow_copy_constructible_v<T>);
}

namespace lcf {

    template <typename T, std::unsigned_integral SizeType = std::uint32_t, typename Allocator = std::allocator<std::byte>>
    class FlexArray
    {
        using Self = FlexArray;
        using ByteAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;
        using ByteAlloctorTraits = std::allocator_traits<ByteAllocator>;
        static constexpr std::size_t k_size_offset_bytes = sizeof(SizeType);
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
        ~FlexArray() noexcept { this->clear(); this->deallocateBlockAndSetPointers(); }
        constexpr FlexArray() noexcept = default;
        explicit FlexArray(const Allocator & alloc) noexcept : m_allocator(alloc) {}
        explicit FlexArray(std::size_t count, const Allocator & alloc = Allocator()) noexcept;
        FlexArray(std::size_t count, const T & value, const Allocator & alloc = Allocator()) noexcept;
        template <std::input_iterator InputIt>
        FlexArray(InputIt first, InputIt last, const Allocator & alloc = Allocator()) noexcept;
        template <std::ranges::input_range Range>
        FlexArray(std::from_range_t, Range && range, const Allocator & alloc = Allocator()) noexcept;
        FlexArray(std::initializer_list<T> init, const Allocator & alloc = Allocator()) noexcept : m_allocator(alloc) { this->assign(std::move(init)); }
        FlexArray(const Self & other) noexcept { this->copyFrom(other); }
        FlexArray(const Self & other, const Allocator & alloc) noexcept : m_allocator(alloc) { this->copyFrom(other); }
        FlexArray(Self && other) noexcept { this->stealFrom(other); }
        FlexArray(Self && other, const Allocator & alloc) noexcept : m_allocator(alloc) { this->stealFrom(other); }
        Self & operator=(const Self & other) noexcept;
        Self & operator=(Self && other) noexcept;
        Self & operator=(std::initializer_list<T> init) noexcept { this->assign_range(init); return *this; }
        constexpr auto operator<=>(const Self & other) const noexcept { return std::lexicographical_compare_three_way(m_first_p, m_last_p, other.m_first_p, other.m_last_p); }
        constexpr bool operator==(const Self & other) const noexcept { return this->size() == other.size() and std::equal(m_first_p, m_last_p, other.m_first_p); }
        T & operator[](std::size_t i) noexcept { return m_first_p[i]; }
        const T & operator[](std::size_t i) const noexcept { return m_first_p[i]; }
    public:
        T & front() noexcept { return *m_first_p; }
        const T & front() const noexcept { return *m_first_p; }
        T & back() noexcept { return *(m_last_p - 1); }
        const T & back() const noexcept { return *(m_last_p - 1); }
        iterator begin() noexcept { return m_first_p; }
        const_iterator begin() const noexcept { return m_first_p; }
        iterator end() noexcept { return m_last_p; }
        const_iterator end() const noexcept { return m_last_p; }
        const_iterator cbegin() const noexcept { return m_first_p; }
        const_iterator cend() const noexcept { return m_last_p; }
        reverse_iterator rbegin() noexcept { return reverse_iterator(m_last_p); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(m_last_p); }
        reverse_iterator rend() noexcept { return reverse_iterator(m_first_p); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(m_first_p); }
        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(m_last_p); }
        const_reverse_iterator crend() const noexcept { return const_reverse_iterator(m_first_p); }
        allocator_type get_allocator() const noexcept { return m_allocator; }
        std::span<T> data_span() noexcept { return {m_first_p, this->size()}; }
        std::span<const T> data_span() const noexcept { return {m_first_p, this->size()}; }
        std::span<const std::byte> counted_bytes() const noexcept;
        std::size_t size() const noexcept { return static_cast<std::size_t>(m_last_p - m_first_p); }
        std::size_t capacity() const noexcept { return static_cast<std::size_t>(m_end_p - m_first_p); }
        bool empty() const noexcept { return m_first_p == m_last_p; }
        void swap(Self & other) noexcept { std::swap(m_first_p, other.m_first_p); std::swap(m_last_p, other.m_last_p); std::swap(m_end_p, other.m_end_p); }
        constexpr iterator push_back(const T & value) noexcept { return this->emplace_back(value); }
        constexpr iterator push_back(T && value) noexcept { return this->emplace_back(std::forward<T>(value)); }
        template <typename... Args>
        constexpr iterator emplace_back(Args &&... args) noexcept;
        constexpr iterator insert(const_iterator pos, const T & value) noexcept { return this->emplace(pos, value); }
        constexpr iterator insert(const_iterator pos, T && value) noexcept { return this->emplace(pos, std::forward<T>(value)); }
        template <typename... Args>
        constexpr iterator emplace(const_iterator pos, Args &&... args) noexcept;
        template <std::ranges::input_range Range>
        constexpr iterator insert_range(const_iterator pos, Range && range) noexcept;
        constexpr iterator erase(const_iterator pos) noexcept { return this->erase(pos, pos + 1); }
        constexpr iterator erase(const_iterator first, const_iterator last) noexcept;
        void pop_back() noexcept;
        void clear() noexcept;
        void clear_and_shrink() noexcept { this->deallocateBlockAndSetPointers(); }
        void shrink_to_fit() noexcept { if (this->capacity() > this->size()) { this->tryReallocateTo(this->size()); } }
        void resize(std::size_t count) noexcept;
        void resize(std::size_t count, const T & value) noexcept;
        void reserve(std::size_t count) noexcept { if (count > this->capacity()) { this->tryReallocateTo(count); } }
        template <std::ranges::input_range Range>
        constexpr void assign_range(Range && range) noexcept { this->clear(); this->append_range(std::forward<Range>(range)); }
        template <std::input_iterator InputIt>
        constexpr void assign(InputIt first, InputIt last) noexcept { return this->assign_range(std::ranges::subrange(first, last)); }
        constexpr void assign(std::initializer_list<T> init) noexcept { this->assign_range(std::move(init)); }
        constexpr void assign(std::size_t count, const T & value) noexcept { this->clear(); this->reserve(count); std::uninitialized_fill_n(m_first_p, count, value); m_last_p = m_first_p + count; }
        template <std::ranges::input_range Range>
        iterator append_range(Range && range) noexcept;
    private:
        std::error_code requireExtraSize(std::size_t extra_size = 1u) noexcept;
        std::error_code tryReallocateTo(std::size_t new_capacity) noexcept;
        void deallocateBlockAndSetPointers(T * first_p = nullptr, std::size_t new_size = 0u, std::size_t new_capacity = 0u) noexcept;
        constexpr iterator makeGap(std::size_t pos_index, std::size_t count) noexcept;
        void closeGap(std::size_t pos_index, std::size_t count) noexcept;
        void copyFrom(const Self & other) noexcept;
        void stealFrom(Self & other) noexcept;
    private:
        static constexpr std::size_t get_block_bytes_for(std::size_t count) noexcept { return k_size_offset_bytes + count * sizeof(T); }
        static constexpr std::byte * header_of(T * data_p) noexcept { return reinterpret_cast<std::byte *>(data_p) - k_size_offset_bytes; }
    private:
        T * m_first_p = nullptr;
        T * m_last_p = nullptr;
        T * m_end_p = nullptr;
        Allocator m_allocator;
    };

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <typename... Args>
    inline constexpr auto FlexArray<T, SizeType, Allocator>::emplace_back(Args &&...args) noexcept -> iterator
    {
        if (this->requireExtraSize()) { return nullptr; }
        if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
            return std::construct_at(m_last_p++, std::forward<Args>(args)...);
        } else {
            try { auto p = std::construct_at(m_last_p, std::forward<Args>(args)...); ++m_last_p; return p; }
            catch (...) { return nullptr; }
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline constexpr auto FlexArray<T, SizeType, Allocator>::makeGap(std::size_t pos_index, std::size_t count) noexcept -> iterator
    {
        if (this->size() + count > this->capacity()) {
            std::size_t old_size = this->size();
            std::size_t new_capacity = get_array_grow_count(this->capacity(), old_size + count);
            ByteAllocator alloc(m_allocator);
            std::byte * header_p = ByteAlloctorTraits::allocate(alloc, get_block_bytes_for(new_capacity));
            if (not header_p) { return nullptr; }
            T * new_first_p = reinterpret_cast<T *>(header_p + k_size_offset_bytes);
            if constexpr (is_nothrow_relocate_v<T>) {
                std::uninitialized_move_n(m_first_p, pos_index, new_first_p);
                std::uninitialized_move_n(m_first_p + pos_index, old_size - pos_index, new_first_p + pos_index + count);
            } else {
                try {
                    std::uninitialized_copy_n(m_first_p, pos_index, new_first_p);
                    std::uninitialized_copy_n(m_first_p + pos_index, old_size - pos_index, new_first_p + pos_index + count);
                } catch (...) {
                    std::destroy_n(new_first_p, pos_index);
                    ByteAlloctorTraits::deallocate(alloc, header_p, get_block_bytes_for(new_capacity));
                    return nullptr;
                }
            }
            this->deallocateBlockAndSetPointers(new_first_p, old_size + count, new_capacity);
        } else {
            T * src = m_first_p + pos_index;
            std::size_t tail_count = this->size() - pos_index;
            std::size_t move_count = (tail_count < count) ? tail_count : count;
            std::size_t backward_n = tail_count - move_count;
            if constexpr (is_nothrow_relocate_v<T>) {
                std::uninitialized_move_n(src + backward_n, move_count, src + tail_count);
                if (backward_n) { std::move_backward(src, src + backward_n, src + tail_count); }
            } else {
                try {
                    std::uninitialized_copy_n(src + backward_n, move_count, src + tail_count);
                    if (backward_n) { std::copy_backward(src, src + backward_n, src + tail_count); }
                } catch (...) { return nullptr; }
            }
            if constexpr (not std::is_trivially_destructible_v<T>) { std::destroy_n(src, move_count); }
            m_last_p += count;
        }
        return m_first_p + pos_index;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline void FlexArray<T, SizeType, Allocator>::closeGap(std::size_t pos_index, std::size_t count) noexcept
    {
        T * dst = m_first_p + pos_index;
        std::move(dst + count, m_last_p, dst);
        if constexpr (not std::is_trivially_destructible_v<T>) { std::destroy(m_last_p - count, m_last_p); }
        m_last_p -= count;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <typename... Args>
    inline constexpr auto FlexArray<T, SizeType, Allocator>::emplace(const_iterator pos, Args &&...args) noexcept -> iterator
    {
        std::size_t pos_index = static_cast<std::size_t>(pos - m_first_p);
        T * gap = this->makeGap(pos_index, 1);
        if (not gap) { return nullptr; }
        if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
            std::construct_at(gap, std::forward<Args>(args)...);
        } else {
            try { std::construct_at(gap, std::forward<Args>(args)...); }
            catch (...) { this->closeGap(pos_index, 1); return nullptr; }
        }
        return gap;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::ranges::input_range Range>
    inline constexpr auto FlexArray<T, SizeType, Allocator>::insert_range(const_iterator pos, Range && range) noexcept -> iterator
    {
        std::size_t pos_index = static_cast<std::size_t>(pos - m_first_p);
        if constexpr (std::ranges::forward_range<Range>) {
            std::size_t count = static_cast<std::size_t>(std::ranges::distance(range));
            T * gap = this->makeGap(pos_index, count);
            if (not gap) { return nullptr; }
            auto write = [&]() {
                if constexpr (std::is_rvalue_reference_v<std::ranges::range_reference_t<Range>>) {
                    std::ranges::uninitialized_move(std::forward<Range>(range), std::ranges::subrange(gap, gap + count));
                } else {
                    std::ranges::uninitialized_copy(std::forward<Range>(range), std::ranges::subrange(gap, gap + count));
                }
            };
            if constexpr (is_nothrow_copy_v<T> && is_nothrow_move_v<T>) {
                write();
            } else {
                try { write(); }
                catch (...) { this->closeGap(pos_index, count); return nullptr; }
            }
            return gap;
        } else {
            std::size_t old_size = this->size();
            if (not this->append_range(std::forward<Range>(range))) { return nullptr; }
            if constexpr (is_nothrow_relocate_v<T>) {
                std::rotate(m_first_p + pos_index, m_first_p + old_size, m_last_p);
            } else {
                try { std::rotate(m_first_p + pos_index, m_first_p + old_size, m_last_p); }
                catch (...) { return nullptr; }
            }
            return m_first_p + pos_index;
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline constexpr auto FlexArray<T, SizeType, Allocator>::erase(const_iterator first, const_iterator last) noexcept -> iterator
    {
        std::size_t erase_count = static_cast<std::size_t>(last - first);
        std::move(const_cast<T *>(last), m_last_p, const_cast<T *>(first));
        if constexpr (not std::is_trivially_destructible_v<T>) { std::destroy(m_last_p - erase_count, m_last_p); }
        m_last_p -= erase_count;
        return const_cast<T *>(first);
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::pop_back() noexcept
    {
        if (m_last_p == m_first_p) { return; }
        if constexpr (std::is_trivially_destructible_v<T>) { --m_last_p; }
        else { std::destroy_at(--m_last_p); }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::clear() noexcept
    {
        if constexpr (not std::is_trivially_destructible_v<T>) { std::destroy_n(m_first_p, this->size()); }
        m_last_p = m_first_p;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::ranges::input_range Range>
    auto FlexArray<T, SizeType, Allocator>::append_range(Range && range) noexcept -> iterator
    {
        if constexpr (std::ranges::forward_range<Range>) {
            std::size_t count = static_cast<std::size_t>(std::ranges::distance(range));
            if (this->requireExtraSize(count)) { return nullptr; }
            iterator start = m_last_p;
            auto write = [&]() {
                if constexpr (std::is_rvalue_reference_v<decltype(*std::ranges::begin(range))>) {
                    std::ranges::uninitialized_move(std::forward<Range>(range), std::ranges::subrange(m_last_p, m_last_p + count));
                } else {
                    std::ranges::uninitialized_copy(std::forward<Range>(range), std::ranges::subrange(m_last_p, m_last_p + count));
                }
                m_last_p += count;
            };
            if constexpr (is_nothrow_copy_v<T> && is_nothrow_move_v<T>) {
                write();
            } else {
                try { write(); }
                catch (...) { return nullptr; }
            }
            return start;
        } else {
            std::size_t start_index = this->size();
            for (auto && elem : range) {
                if (not this->emplace_back(std::forward<decltype(elem)>(elem))) { return nullptr; }
            }
            return m_first_p + start_index;
        }
    }
    
    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    FlexArray<T, SizeType, Allocator>::FlexArray(std::size_t count, const Allocator & alloc) noexcept : m_allocator(alloc)
    {
        this->requireExtraSize(count);
        m_last_p = m_first_p + count;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    FlexArray<T, SizeType, Allocator>::FlexArray(std::size_t count, const T & value, const Allocator & alloc) noexcept : m_allocator(alloc)
    {
        this->requireExtraSize(count);
        std::uninitialized_fill_n(m_first_p, count, value);
        m_last_p = m_first_p + count;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::input_iterator InputIt>
    FlexArray<T, SizeType, Allocator>::FlexArray(InputIt first, InputIt last, const Allocator & alloc) noexcept : m_allocator(alloc)
    {
        if constexpr (std::forward_iterator<InputIt>) {
            std::size_t count = static_cast<std::size_t>(std::distance(first, last));
            this->requireExtraSize(count);
            std::uninitialized_copy(first, last, m_first_p);
            m_last_p = m_first_p + count;
        } else {
            for (auto it = first; it != last; ++it) { this->emplace_back(*it); }
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    template <std::ranges::input_range Range>
    FlexArray<T, SizeType, Allocator>::FlexArray(std::from_range_t, Range && range, const Allocator & alloc) noexcept : m_allocator(alloc)
    {
        this->append_range(std::forward<Range>(range));
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::operator=(const Self & other) noexcept -> Self &
    {
        if (this == &other) { return *this; }
        this->clear();
        this->copyFrom(other);
        return *this;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    auto FlexArray<T, SizeType, Allocator>::operator=(Self && other) noexcept -> Self &
    {
        if (this == &other) { return *this; }
        this->deallocateBlockAndSetPointers();
        this->stealFrom(other);
        return *this;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::resize(std::size_t count) noexcept
    {
        if constexpr (not std::is_trivially_destructible_v<T>) {
            if (count < this->size()) { std::destroy(m_first_p + count, m_last_p); }
        }
        if (count > this->size()) { this->requireExtraSize(count - this->size()); }
        m_last_p = m_first_p + count;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    void FlexArray<T, SizeType, Allocator>::resize(std::size_t count, const T & value) noexcept
    {
        if constexpr (not std::is_trivially_destructible_v<T>) {
            if (count < this->size()) { std::destroy(m_first_p + count, m_last_p); }
        }
        if (count > this->size()) {
            this->requireExtraSize(count - this->size());
            if constexpr (is_nothrow_copy_v<T>) {
                std::uninitialized_fill_n(m_last_p, count - this->size(), value);
            } else {
                try { std::uninitialized_fill_n(m_last_p, count - this->size(), value); }
                catch (...) {}
            }
        }
        m_last_p = m_first_p + count;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline std::span<const std::byte> FlexArray<T, SizeType, Allocator>::counted_bytes() const noexcept
    {
        if (not m_first_p) { return {}; }
        auto header_p = header_of(m_first_p);
        *reinterpret_cast<SizeType *>(header_p) = static_cast<SizeType>(this->size());
        return {header_p, get_block_bytes_for(this->size())};
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline std::error_code FlexArray<T, SizeType, Allocator>::requireExtraSize(std::size_t extra_size) noexcept
    {
        std::size_t required_size = this->size() + extra_size;
        std::size_t capacity = this->capacity();
        if (required_size <= capacity) [[likely]] { return {}; }
        return this->tryReallocateTo(::get_array_grow_count(capacity, required_size));
    }

    template<typename T, std::unsigned_integral SizeType, typename Allocator>
    inline std::error_code FlexArray<T, SizeType, Allocator>::tryReallocateTo(std::size_t new_capacity) noexcept
    {
        ByteAllocator alloc(m_allocator);
        std::byte * header_p = ByteAlloctorTraits::allocate(alloc, get_block_bytes_for(new_capacity));
        if (not header_p) { return std::make_error_code(std::errc::not_enough_memory); }
        T * new_first_p = reinterpret_cast<T *>(header_p + k_size_offset_bytes);
        std::size_t current_size = this->size();
        if constexpr (is_nothrow_relocate_v<T>) {
            std::uninitialized_move_n(m_first_p, current_size, new_first_p);
        } else {
            try { std::uninitialized_copy_n(m_first_p, current_size, new_first_p); }
            catch (...) {
                ByteAlloctorTraits::deallocate(alloc, header_p, get_block_bytes_for(new_capacity));
                return std::make_error_code(std::errc::operation_canceled);
            }
        }
        this->deallocateBlockAndSetPointers(new_first_p, current_size, new_capacity);
        return {};
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline void FlexArray<T, SizeType, Allocator>::deallocateBlockAndSetPointers(T * first_p, std::size_t new_size, std::size_t new_capacity) noexcept
    {
        if (m_first_p) {
            if constexpr (not std::is_trivially_destructible_v<T>) { std::destroy_n(m_first_p, this->size()); }
            ByteAllocator alloc(m_allocator);
            ByteAlloctorTraits::deallocate(alloc, header_of(m_first_p), get_block_bytes_for(this->capacity()));
        }
        m_first_p = first_p;
        m_last_p = first_p + new_size;
        m_end_p = first_p + new_capacity;
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline void FlexArray<T, SizeType, Allocator>::copyFrom(const Self &other) noexcept
    {
        std::size_t size = other.size();
        this->requireExtraSize(size);
        if constexpr (is_nothrow_copy_v<T>) {
            std::uninitialized_copy_n(other.m_first_p, size, m_first_p);
            m_last_p = m_first_p + size;
        } else {
            try { std::uninitialized_copy_n(other.m_first_p, size, m_first_p); m_last_p = m_first_p + size; }
            catch (...) {}
        }
    }

    template <typename T, std::unsigned_integral SizeType, typename Allocator>
    inline void FlexArray<T, SizeType, Allocator>::stealFrom(Self &other) noexcept
    {
        m_first_p = std::exchange(other.m_first_p, nullptr);
        m_last_p = std::exchange(other.m_last_p, nullptr);
        m_end_p = std::exchange(other.m_end_p, nullptr);
    }
}
