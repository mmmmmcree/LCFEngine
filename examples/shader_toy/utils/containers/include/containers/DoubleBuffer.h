#pragma once

#include "details/cache_line_size.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

namespace lcf::shader_toy {

template <typename T, typename Allocator = std::allocator<T>>
class DoubleBuffer
{
    using Self = DoubleBuffer;
    using Buffer = std::vector<T, Allocator>;
public:
    DoubleBuffer() noexcept = default;
    DoubleBuffer(const Self &) = delete;
    DoubleBuffer(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    void push(T value) noexcept { m_buffers[m_write_index].emplace_back(std::move(value)); }
    Buffer drain() noexcept
    {
        const std::uint32_t read_index = m_read_index.load(std::memory_order_acquire);
        Buffer values = std::move(m_buffers[read_index]);
        m_buffers[read_index].clear();
        return values;
    }
    void publish() noexcept
    {
        if (m_buffers[m_write_index].empty()) { return; }
        m_write_index = m_read_index.exchange(m_write_index, std::memory_order_acq_rel);
    }
private:
    alignas(lcf::details::k_cache_line_size) std::array<Buffer, 2> m_buffers {};
    alignas(lcf::details::k_cache_line_size) std::atomic<std::uint32_t> m_read_index {0};
    alignas(lcf::details::k_cache_line_size) std::uint32_t m_write_index = 1;
};

} // namespace lcf::shader_toy
