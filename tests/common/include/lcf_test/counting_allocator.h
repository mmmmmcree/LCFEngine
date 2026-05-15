#pragma once

#include <atomic>
#include <cstddef>
#include <new>

namespace lcf::test {

    // Tracks alloc/dealloc pair counts and outstanding bytes.
    struct AllocStats
    {
        std::atomic<std::size_t> alloc_calls{0};
        std::atomic<std::size_t> dealloc_calls{0};
        std::atomic<std::size_t> bytes_outstanding{0};
        void reset() noexcept
        {
            alloc_calls.store(0);
            dealloc_calls.store(0);
            bytes_outstanding.store(0);
        }
    };

    // Minimal allocator that records every allocate/deallocate call.
    template <typename T>
    struct CountingAllocator
    {
        using value_type = T;
        AllocStats * stats_p = nullptr;

        CountingAllocator() = default;
        explicit CountingAllocator(AllocStats * stats) noexcept : stats_p(stats) {}
        template <typename U>
        CountingAllocator(const CountingAllocator<U> & other) noexcept : stats_p(other.stats_p) {}

        T * allocate(std::size_t n)
        {
            if (stats_p) {
                ++stats_p->alloc_calls;
                stats_p->bytes_outstanding += n * sizeof(T);
            }
            return static_cast<T *>(::operator new(n * sizeof(T)));
        }
        void deallocate(T * p, std::size_t n) noexcept
        {
            if (stats_p) {
                ++stats_p->dealloc_calls;
                stats_p->bytes_outstanding -= n * sizeof(T);
            }
            ::operator delete(p);
        }
        template <typename U>
        bool operator==(const CountingAllocator<U> & other) const noexcept { return stats_p == other.stats_p; }
    };

} // namespace lcf::test
