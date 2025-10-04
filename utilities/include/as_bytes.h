#pragma once

#include "concepts/span_convertible_concept.h"
#include <span>

namespace lcf {
    using ByteView = std::span<const std::byte>;

    template <typename T, std::size_t Extent>
    auto span_as_bytes(std::span<T, Extent> span)
    {
        if constexpr (std::is_const_v<T>) {
            return std::as_bytes(span);
        } else {
            return std::as_writable_bytes(span);
        }
    }

    template <span_convertible_c SpanConvertible>
    auto as_bytes(SpanConvertible & data)
    {
        return span_as_bytes(std::span(data));
    }

    template <typename T>
    auto as_bytes_from_ptr(T * data_p, size_t element_count)
    {
        return span_as_bytes(std::span(data_p, element_count));
    }

    template <typename T>
    auto as_bytes_from_value(T & data)
    {
        return span_as_bytes(std::span(&data, 1));
    }
}