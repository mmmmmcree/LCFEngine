#pragma once

#include <span>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>

namespace lcf::shader_core {

    class BufferReader
    {
    public:
        explicit BufferReader(std::span<const std::byte> data) noexcept : m_data(data) {}

        template <typename T>
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
    public:
        BufferWriter() noexcept = default;
        explicit BufferWriter(size_t reserve) { m_buffer.reserve(reserve); }

        template <typename T>
        void write(const T & value) noexcept
        {
            const auto * src = reinterpret_cast<const std::byte *>(&value);
            m_buffer.insert(m_buffer.end(), src, src + sizeof(T));
        }

        void writeBytes(std::span<const std::byte> bytes) noexcept
        {
            m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
        }

        template <typename T>
        size_t reserveSlot() noexcept
        {
            size_t offset = m_buffer.size();
            T zero{};
            write(zero);
            return offset;
        }

        template <typename T>
        void patch(size_t offset, const T & value) noexcept
        {
            std::memcpy(m_buffer.data() + offset, &value, sizeof(T));
        }

        size_t size() const noexcept { return m_buffer.size(); }
        const std::vector<std::byte> & buffer() const noexcept { return m_buffer; }
        std::vector<std::byte> && release() noexcept { return std::move(m_buffer); }

    private:
        std::vector<std::byte> m_buffer;
    };

    class FixedBufferWriter
    {
    public:
        explicit FixedBufferWriter(std::span<std::byte> buffer) noexcept : m_buffer(buffer) {}

        void writeBytes(std::span<const std::byte> bytes) noexcept
        {
            std::memcpy(m_buffer.data() + m_offset, bytes.data(), bytes.size());
            m_offset += bytes.size();
        }

        size_t offset() const noexcept { return m_offset; }

    private:
        std::span<std::byte> m_buffer;
        size_t m_offset = 0;
    };

}
