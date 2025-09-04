#pragma once

#include <bit>
#include <type_traits>

namespace lcf {
    template <typename T, size_t alignment>
    struct AlignmentInfoOf
    {
        using type = std::remove_cvref_t<T>;
        static constexpr size_t s_type_size = sizeof(type);
        static constexpr size_t s_value = std::bit_ceil(alignment);
    };
    
    template <typename T>
    using STDAlignmentInfoOf = AlignmentInfoOf<T, std::alignment_of_v<T>>;
}