#pragma once

#include "concepts/span_convertible_concept.h"
#include "concepts/standard_layout_concept.h"
#include <span>

namespace lcf {
    using ByteView = std::span<const std::byte>;
    using WritableByteView = std::span<std::byte>;

    template <standard_layout_c T, std::size_t Extent>
    constexpr auto span_as_bytes(std::span<T, Extent> span) noexcept
    {
        if constexpr (std::is_const_v<T>) {
            return std::as_bytes(span);
        } else {
            return std::as_writable_bytes(span);
        }
    }

    template <span_convertible_c SpanConvertible>
    constexpr auto as_bytes(SpanConvertible & data) noexcept
    {
        return span_as_bytes(std::span(data));
    }

    template <standard_layout_c T>
    constexpr auto as_bytes_from_ptr(T * data_p, size_t element_count) noexcept // element_count is size_in_bytes if T is void
    {
        if constexpr (std::is_void_v<std::remove_cvref_t<T>>) {
            using ByteType = std::conditional_t<std::is_const_v<T>, const std::byte, std::byte>;
            return span_as_bytes(std::span(static_cast<ByteType *>(data_p), element_count));
        } else {
            return span_as_bytes(std::span(data_p, element_count));
        }
    }

    template <standard_layout_c T>
    constexpr auto as_bytes_from_value(T & data) noexcept
    {
        return span_as_bytes(std::span(&data, 1));
    }

    template <standard_layout_c T>
    std::span<const T> bytes_as_span(ByteView bytes) noexcept
    {
        assert(bytes.size() % sizeof(T) == 0);
        return std::span<const T>(reinterpret_cast<const T *>(bytes.data()), bytes.size() / sizeof(T));
    }

    template <standard_layout_c T>
    std::span<T> bytes_as_span(WritableByteView bytes) noexcept
    {
        assert(bytes.size() % sizeof(T) == 0);
        return std::span<T>(reinterpret_cast<T *>(bytes.data()), bytes.size() / sizeof(T));
    }
}