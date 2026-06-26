#pragma once

#include <array>
#include <atomic>
#include <optional>
#include <concepts>
#include <new>

namespace lcf {

template <std::default_initializable T, bool allow_multiple_reads = true>
class SPSCValue
{
    struct TaggedIndex
    {
        TaggedIndex(uint8_t index, bool tag = false) noexcept { this->setTagAndIndex(index, tag); }
        uint8_t index() const noexcept { return m_index & 0x07; }
        bool isConsumable() const noexcept { return m_index & 0x08; }
        void setTagAndIndex(uint8_t index, bool tag) noexcept { m_index = index | ( tag ? 0x08 : 0x00 ); }
        uint8_t m_index;
    };
    struct alignas(std::hardware_destructive_interference_size) CacheAlignedValue
    {
        T m_value;
    };
    using Buffer = std::array<CacheAlignedValue, 3>;
public:
    using Self = SPSCValue;
    ~SPSCValue() noexcept = default;
    SPSCValue() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        if constexpr (allow_multiple_reads) {
            m_write_index = {1};
            m_available_index.store({0, true}, std::memory_order_relaxed);
            m_buffer[0].m_value = {};
        }
    }
    explicit SPSCValue(T value) noexcept(std::is_nothrow_move_constructible_v<T>) :
        m_write_index {1},
        m_available_index {{ 0, true, }}
    {
        m_buffer[0].m_value = std::move(value);
    }
    SPSCValue(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    SPSCValue(Self &&) = delete;
    Self & operator=(Self &&) = delete;
public:
    void write(T value) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        m_buffer[m_write_index.index()].m_value = std::move(value);
        this->swapWriteBuffer();
    }
    T read() noexcept(std::is_nothrow_default_constructible_v<T> and std::is_nothrow_copy_assignable_v<T>) requires allow_multiple_reads
    {
        this->swapReadBuffer();
        return m_buffer[m_read_index.index()].m_value;
    }
    std::optional<T> read() noexcept(std::is_nothrow_move_constructible_v<T>) requires (not allow_multiple_reads)
    {
        if (not this->swapReadBuffer()) { return std::nullopt; }
        return std::move(m_buffer[m_read_index.index()].m_value);
    }
private:
    void swapWriteBuffer() noexcept
    {
        TaggedIndex old_available_index = m_available_index.exchange({m_write_index.index(), true}, std::memory_order_release);
        m_write_index.setTagAndIndex(old_available_index.index(), false);
    }
    bool swapReadBuffer() noexcept
    {
        TaggedIndex new_available_index = m_read_index;
        TaggedIndex current_available_index = m_available_index.load(std::memory_order_acquire);
        if (not current_available_index.isConsumable()) { return false; }
        current_available_index = m_available_index.exchange(new_available_index, std::memory_order_acquire);
        m_read_index = {current_available_index.index(), false};
        return true;
    }
private:
    Buffer m_buffer;
    alignas(std::hardware_destructive_interference_size) TaggedIndex m_write_index { 0 };
    alignas(std::hardware_destructive_interference_size) std::atomic<TaggedIndex> m_available_index { 1 };
    alignas(std::hardware_destructive_interference_size) TaggedIndex m_read_index { 2 };
};

} // namespace lcf