#pragma once

#include "concepts/span_convertible_concept.h"

namespace lcf {
    template<typename T, span_convertible_c SpanConvertible>
    auto span_cast(SpanConvertible & span_convertible) noexcept
    {
        auto span = std::span(span_convertible);
        using SrcType = typename decltype(span)::element_type;
        using DstType = std::conditional_t<std::is_const_v<SrcType>, const T, T>;
        return std::span<DstType>(reinterpret_cast<DstType *>(span.data()), span.size_bytes() / sizeof(DstType));
    }
}