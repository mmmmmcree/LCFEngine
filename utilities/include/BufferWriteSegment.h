#pragma once

#include "as_bytes.h"
#include <deque>
#include <limits>

namespace lcf {
    class BufferWriteSegment
    {
    public:
        BufferWriteSegment() = default;
        BufferWriteSegment(ByteView bytes, size_t offset_in_bytes = 0u) noexcept :
            m_data(bytes), m_offset_in_bytes(offset_in_bytes) {}
        BufferWriteSegment(const BufferWriteSegment& other) = default;
        BufferWriteSegment(BufferWriteSegment&& other) noexcept = default;
        BufferWriteSegment& operator=(const BufferWriteSegment& other) = default;
        ByteView getDataView() const noexcept { return m_data; }
        const std::byte * getData() const noexcept { return m_data.data(); }
        size_t getSizeInBytes() const noexcept { return m_data.size_bytes(); }
        size_t getBeginOffsetInBytes() const noexcept { return m_offset_in_bytes; }
        size_t getEndOffsetInBytes() const noexcept { return m_offset_in_bytes + m_data.size_bytes(); }
        bool operator==(const BufferWriteSegment& other) const noexcept
        {
            return m_offset_in_bytes == other.m_offset_in_bytes
                and m_data.data() == other.m_data.data()
                and m_data.size() == other.m_data.size();
        }
        BufferWriteSegment & operator+=(const BufferWriteSegment& other) noexcept //merge rule for boost::icl::interval_map
        {
            return *this;
        }
    private:
        ByteView m_data;
        size_t m_offset_in_bytes = 0;
    };
}

namespace lcf::impl {
    template <typename Segments>
    class BufferWriteSegmentsIMPL;

    template <>
    class BufferWriteSegmentsIMPL<std::deque<BufferWriteSegment>>
    {
        using Self = BufferWriteSegmentsIMPL<std::deque<BufferWriteSegment>>;
    public:
        using SegmentContainer = std::deque<BufferWriteSegment>;
        BufferWriteSegmentsIMPL() = default;
        bool isValid() const noexcept { return m_write_segment_lower_bound < m_write_segment_upper_bound; }
        Self & add(const BufferWriteSegment& segment) noexcept
        {
            m_segments.emplace_back(segment);
            m_write_segment_lower_bound = std::min(m_write_segment_lower_bound, segment.getBeginOffsetInBytes());
            m_write_segment_upper_bound = std::max(m_write_segment_upper_bound, segment.getEndOffsetInBytes());
            return *this;
        }
        Self & addIfAbsent(const BufferWriteSegment& segment) noexcept
        {
            m_segments.emplace_front(segment);
            m_write_segment_lower_bound = std::min(m_write_segment_lower_bound, segment.getBeginOffsetInBytes());
            m_write_segment_upper_bound = std::max(m_write_segment_upper_bound, segment.getEndOffsetInBytes());
            return *this;
        }
        bool empty() const noexcept { return m_segments.empty(); }
        auto begin()  noexcept { return m_segments.begin(); }
        auto begin() const noexcept { return m_segments.begin(); }
        auto end()  noexcept { return m_segments.end(); }
        auto end() const noexcept { return m_segments.end(); }
        void clear() noexcept
        {
            m_segments.clear();
            m_write_segment_lower_bound = std::numeric_limits<size_t>::max();
            m_write_segment_upper_bound = 0u;
        }
        size_t getLowerBoundInBytes() const noexcept { return m_write_segment_lower_bound; }
        size_t getUpperBoundInBytes() const noexcept { return m_write_segment_upper_bound; }
        size_t getValidSizeInBytes() const noexcept { return m_write_segment_upper_bound - m_write_segment_lower_bound; }
    private:
        SegmentContainer m_segments;
        size_t m_write_segment_lower_bound = std::numeric_limits<size_t>::max();
        size_t m_write_segment_upper_bound = 0u;
    };
}

namespace lcf {
    using BufferWriteSegments = lcf::impl::BufferWriteSegmentsIMPL<std::deque<BufferWriteSegment>>;
}