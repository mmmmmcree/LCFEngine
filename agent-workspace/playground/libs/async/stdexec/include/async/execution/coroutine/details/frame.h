#pragma once

#include <async/dependencies.h>

namespace lcf::async::execution::details {

struct alignas(std::max_align_t) FrameBlock
{
    explicit FrameBlock(std::pmr::memory_resource * resource_p) noexcept : m_resource_p(resource_p) {}
    union
    {
        FrameBlock * m_next_p;
        std::pmr::memory_resource * m_resource_p;
    };
};

class FrameCache
{
    using Self = FrameCache;
    static constexpr std::size_t k_min_size = 128;
    static constexpr std::size_t k_max_size = 8192;
    static constexpr std::size_t k_max_bytes = 64 * 1024;
    static constexpr std::size_t k_size_step = 64;
    static constexpr std::size_t k_bin_count = (k_max_size - k_min_size) / k_size_step + 1;
    using FreeLists = std::array<FrameBlock *, k_bin_count>;
public:
    ~FrameCache() noexcept { this->clear(); Self::is_retired() = true; }
    FrameCache() noexcept = default;
    FrameCache(const Self &) = delete;
    FrameCache(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    static Self & instance() noexcept
    {
        thread_local Self cache;
        return cache;
    }
    static FrameBlock * allocate_block(std::size_t size)
    {
        if (size > k_max_size) { return Self::make_block(size); }
        const auto capacity = Self::block_capacity(size);
        if (Self::is_retired()) { return Self::make_block(capacity); }
        return Self::instance().allocate(capacity);
    }
    static void deallocate_block(FrameBlock * block_p, std::size_t size) noexcept
    {
        if (size > k_max_size or Self::is_retired()) { ::operator delete(block_p); }
        else { Self::instance().deallocate(block_p, Self::block_capacity(size)); }
    }
    FrameBlock * allocate(std::size_t size)
    {
        const auto bin = Self::bin_index(size);
        auto * block_p = m_free_lists[bin];
        if (not block_p) { return Self::make_block(size); }
        m_free_lists[bin] = block_p->m_next_p;
        m_cached_bytes -= size;
        block_p->m_resource_p = nullptr;
        return block_p;
    }
    void deallocate(FrameBlock * block_p, std::size_t size) noexcept
    {
        while (m_cached_bytes + size > k_max_bytes) { this->evictLargest(); }
        const auto bin = Self::bin_index(size);
        block_p->m_next_p = m_free_lists[bin];
        m_free_lists[bin] = block_p;
        m_cached_bytes += size;
        m_largest_bin = std::max(m_largest_bin, bin);
    }
    void clear() noexcept
    {
        for (auto * & head_p : m_free_lists) {
            while (head_p) {
                auto * block_p = head_p;
                head_p = block_p->m_next_p;
                ::operator delete(block_p);
            }
        }
        m_cached_bytes = 0;
        m_largest_bin = 0;
    }
private:
    void evictLargest() noexcept
    {
        while (not m_free_lists[m_largest_bin]) { --m_largest_bin; }
        auto * block_p = m_free_lists[m_largest_bin];
        m_free_lists[m_largest_bin] = block_p->m_next_p;
        m_cached_bytes -= k_min_size + m_largest_bin * k_size_step;
        ::operator delete(block_p);
    }
    static std::size_t bin_index(std::size_t size) noexcept
    {
        return (size - k_min_size) / k_size_step;
    }
    static std::size_t block_capacity(std::size_t size) noexcept
    {
        return std::max(k_min_size, (size + k_size_step - 1) / k_size_step * k_size_step);
    }
    static bool & is_retired() noexcept
    {
        thread_local bool retired = false;
        return retired;
    }
    static FrameBlock * make_block(std::size_t size)
    {
        return ::new (::operator new(size)) FrameBlock {nullptr};
    }
private:
    FreeLists m_free_lists {};
    std::size_t m_cached_bytes {0};
    std::size_t m_largest_bin {0};
};

inline std::pmr::memory_resource * & current_frame_resource() noexcept
{
    thread_local std::pmr::memory_resource * resource_p = nullptr;
    return resource_p;
}

inline void * allocate_frame(std::size_t size, std::size_t alignment)
{
    alignment = std::max(alignment, alignof(FrameBlock));
    const auto prefix = std::max(sizeof(FrameBlock), alignment);
    if (size > std::numeric_limits<std::size_t>::max() - prefix) { throw std::bad_alloc(); }
    const auto total = size + prefix;
    if (auto * resource_p = current_frame_resource()) {
        auto * base_p = static_cast<std::byte *>(resource_p->allocate(total, alignment));
        auto * frame_p = base_p + prefix;
        ::new (frame_p - sizeof(FrameBlock)) FrameBlock {resource_p};
        return frame_p;
    }
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        auto * base_p = static_cast<std::byte *>(::operator new(total, std::align_val_t {alignment}));
        auto * frame_p = base_p + prefix;
        ::new (frame_p - sizeof(FrameBlock)) FrameBlock {nullptr};
        return frame_p;
    }
    return FrameCache::allocate_block(total) + 1;
}

inline void deallocate_frame(void * frame_p, std::size_t size, std::size_t alignment) noexcept
{
    auto * block_p = static_cast<FrameBlock *>(frame_p) - 1;
    alignment = std::max(alignment, alignof(FrameBlock));
    const auto prefix = std::max(sizeof(FrameBlock), alignment);
    auto * base_p = static_cast<std::byte *>(frame_p) - prefix;
    if (block_p->m_resource_p) {
        block_p->m_resource_p->deallocate(base_p, size + prefix, alignment);
    } else if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        ::operator delete(base_p, std::align_val_t {alignment});
    } else {
        FrameCache::deallocate_block(block_p, size + prefix);
    }
}

}
