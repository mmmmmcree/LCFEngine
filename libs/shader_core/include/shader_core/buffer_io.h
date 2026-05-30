#pragma once

#include "bytes.h"
#include "concepts/standard_layout_concept.h"
#include "concepts/trivially_copyable_concept.h"
#include <span>
#include <cstddef>
#include <cstring>
#include <vector>

namespace lcf::sc {

    template <typename T>
    concept binary_io_type_c = trivially_copyable_c<T> and standard_layout_c<T>;

    class BufferReader
    {
    public:
        explicit BufferReader(std::span<const std::byte> data) noexcept : m_data(data) {}
        template <binary_io_type_c T>
        bool read(T & out) noexcept
        {
            if (m_offset + sizeof(T) > m_data.size()) { return false; }
            std::memcpy(&out, m_data.data() + m_offset, sizeof(T));
            m_offset += sizeof(T);
            return true;
        }
        bool readBytes(std::span<std::byte> dst) noexcept
        {
            if (m_offset + dst.size() > m_data.size()) { return false; }
            std::memcpy(dst.data(), m_data.data() + m_offset, dst.size());
            m_offset += dst.size();
            return true;
        }
        bool skip(size_t bytes) noexcept
        {
            if (m_offset + bytes > m_data.size()) { return false; }
            m_offset += bytes;
            return true;
        }
        size_t offset() const noexcept { return m_offset; }
        size_t remaining() const noexcept { return m_data.size() - m_offset; }
        bool eof() const noexcept { return m_offset >= m_data.size(); }
    private:
        std::span<const std::byte> m_data;
        size_t m_offset = 0;
    };

    class BufferWriter
    {
        using Self = BufferWriter;
        using Buffer = std::vector<std::byte>;
    public:
        BufferWriter() noexcept = default;
        explicit BufferWriter(size_t reserve) { m_buffer.reserve(reserve); }
        template <binary_io_type_c T>
        Self & write(const T & value) noexcept
        {
            m_buffer.append_range(as_const_bytes_from_value(value));
            return *this;
        }
        Self & writeBytes(std::span<const std::byte> bytes) noexcept
        {
            m_buffer.append_range(bytes);
            return *this;
        }
        template <std::integral T>
        Self & writeLengthAndBytes(std::span<const std::byte> bytes) noexcept
        {
            this->write(static_cast<T>(bytes.size()))
               .writeBytes(bytes);
            return *this;
        }
        template <binary_io_type_c T>
        size_t reserveSlot() noexcept
        {
            size_t offset = m_buffer.size();
            write(T{});
            return offset;
        }
        template <binary_io_type_c T>
        Self & patch(size_t offset, const T & value) noexcept
        {
            std::memcpy(m_buffer.data() + offset, &value, sizeof(T));
            return *this;
        }
        size_t size() const noexcept { return m_buffer.size(); }
        const Buffer & getBuffer() const noexcept { return m_buffer; }
        Buffer && releaseBuffer() noexcept { return std::move(m_buffer); }
    private:
        Buffer m_buffer;
    };
}
