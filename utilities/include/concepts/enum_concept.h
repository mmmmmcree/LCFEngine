#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept enum_c = std::is_enum_v<T>;

    template <enum_c Enum>
    struct enum_flags_true_type 
    {
        using enum_type = std::remove_cvref_t<Enum>;
        static constexpr bool s_is_flags = true;
    };

    template <enum_c Enum>
    struct enum_flags_false_type
    {
        using enum_type = std::remove_cvref_t<Enum>;
        static constexpr bool s_is_flags = false;
    };

    template <enum_c Enum>
    struct enum_flags_traits : enum_flags_false_type<Enum> { };

    template <enum_c Enum>
    static constexpr bool enum_flags_traits_v = enum_flags_traits<std::remove_cvref_t<Enum>>::s_is_flags;

    template <enum_c Enum>
    using enum_flags_traits_t = typename enum_flags_traits<std::remove_cvref_t<Enum>>::enum_type;

    template <typename T>
    concept enum_flags_c = enum_c<T> and enum_flags_traits_v<T>;

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
}