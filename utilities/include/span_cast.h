#pragma once

#include "lcf_type_traits.h"
#include <ranges>
#include <span>

namespace lcf {
    template <typename Dst, typename Src>
    requires is_pointer_type_convertible_v<std::remove_cv_t<Src>, std::remove_cv_t<Dst>>
    auto pointer_type_cast(Src * src) noexcept
    {
        using result_type = std::conditional_t<std::is_const_v<Src>,
            std::conditional_t<std::is_volatile_v<Src>, const volatile Dst*, const Dst*>,
            std::conditional_t<std::is_volatile_v<Src>, volatile Dst*, Dst*>>;
        return reinterpret_cast<result_type>(src);
    }
}

namespace lcf {
    template <typename T, std::ranges::contiguous_range Range>
    requires is_pointer_type_convertible_v<std::ranges::range_value_t<Range>, T> or std::is_same_v<std::ranges::range_value_t<Range>, T>
    auto span_cast(Range && range) noexcept
    {
        auto span = std::span(std::forward<Range>(range));
        using Src = typename decltype(span)::element_type;
        using Dst = std::conditional_t<std::is_const_v<Src>, const T, T>;
        if constexpr (std::is_same_v<std::ranges::range_value_t<Range>, T>) { return span; }
        else {
            return std::span<Dst>(pointer_type_cast<Dst>(span.data()), span.size_bytes() / sizeof(Dst));
        }
    }
}