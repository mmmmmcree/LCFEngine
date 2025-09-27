#pragma once

#include "concepts/span_convertible_concept.h"
#include <span>

namespace lcf {
    using ByteView = std::span<const std::byte>;

    template <span_convertible_c SpanConvertible>
    auto as_bytes(SpanConvertible & data)
    {
        return std::as_bytes(std::span(data));
    }

    template <typename T>
    auto as_bytes_from_ptr(const T * data_p, size_t size_in_bytes)
    {
        return std::as_bytes(std::span(data_p, size_in_bytes));
    }

    template <typename T>
    auto as_bytes_from_value(T & data)
    {
        return std::as_bytes(std::span(&data, 1));
    }
}