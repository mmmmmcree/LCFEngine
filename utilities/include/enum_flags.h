#pragma once

#include "concepts/enum_concept.h"
#include <bit>

namespace lcf {
    template <enum_flags_c EnumFlags>
    constexpr EnumFlags operator&(const EnumFlags & lhs, const EnumFlags & rhs) noexcept
    {
        using UnderlyingType = std::underlying_type_t<EnumFlags>;
        return static_cast<EnumFlags>(static_cast<UnderlyingType>(lhs) & static_cast<UnderlyingType>(rhs));
    }

    template <enum_flags_c EnumFlags>
    constexpr EnumFlags operator|(const EnumFlags & lhs, const EnumFlags & rhs) noexcept
    {
        using UnderlyingType = std::underlying_type_t<EnumFlags>;
        return static_cast<EnumFlags>(static_cast<UnderlyingType>(lhs) | static_cast<UnderlyingType>(rhs));
    }

    template <enum_flags_c EnumFlags>
    constexpr EnumFlags operator^(const EnumFlags & lhs, const EnumFlags & rhs) noexcept
    {
        using UnderlyingType = std::underlying_type_t<EnumFlags>;
        return static_cast<EnumFlags>(static_cast<UnderlyingType>(lhs) ^ static_cast<UnderlyingType>(rhs));
    }

    template <enum_flags_c EnumFlags>
    constexpr EnumFlags operator~(const EnumFlags & flags) noexcept
    {
        using UnderlyingType = std::underlying_type_t<EnumFlags>;
        return static_cast<EnumFlags>(~static_cast<UnderlyingType>(flags));
    }

    template <enum_flags_c EnumFlags>
    constexpr EnumFlags & operator&=(EnumFlags & lhs, const EnumFlags & rhs) noexcept { return lhs = lhs & rhs; }

    template <enum_flags_c EnumFlags>
    constexpr EnumFlags & operator|=(EnumFlags & lhs, const EnumFlags & rhs) noexcept { return lhs = lhs | rhs; }

    template <enum_flags_c EnumFlags>
    constexpr EnumFlags & operator^=(EnumFlags & lhs, const EnumFlags & rhs) noexcept { return lhs = lhs ^ rhs; }

    #define MAKE_ENUM_FLAGS(EnumFlags) \
        template <> struct enum_flags_traits<EnumFlags> : lcf::enum_flags_true_type<EnumFlags> {}

    template <enum_flags_c EnumFlags>
    constexpr bool is_flag_bit(const EnumFlags & flags) noexcept { return std::has_single_bit(static_cast<std::underlying_type_t<EnumFlags>>(flags)); }
 
    template <enum_flags_c EnumFlags>
    constexpr bool contains_flags(const EnumFlags & flags, const EnumFlags & mask) noexcept { return (flags & mask) == mask; }
}