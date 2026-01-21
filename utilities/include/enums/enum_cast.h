#pragma once

#include "concepts/enum_concept.h"
#include "enum_flags.h"
#include <tuple>

namespace lcf {
    template <enum_c Enum>
    struct enum_category_traits
    {
        using category_type = std::tuple<>;
        constexpr static category_type mappings[] = {{}};
    };

    template <enum_c Enum>
    using enum_category_t = typename enum_category_traits<Enum>::category_type;

    template <enum_c Dst, enum_c Src>
    requires std::same_as<enum_category_t<Dst>, enum_category_t<Src>>
    constexpr Dst enum_cast(Src src) noexcept
    {
        for (const auto & mapping : enum_category_traits<Dst>::mappings) {
            if (std::get<Src>(mapping) == src) {
                return std::get<Dst>(mapping);
            }
        }
        return Dst {};
    }

    template <enum_flags_c Dst, enum_flags_c Src>
    requires std::same_as<enum_category_t<Dst>, enum_category_t<Src>>
    constexpr Dst enum_flags_cast(Src src) noexcept
    {
        Dst dst {};
        for (const auto & mapping : enum_category_traits<Dst>::mappings) {
            if (static_cast<bool>(std::get<Src>(mapping) & src)) {
                dst |= std::get<Dst>(mapping);
            }
        }
        return dst;
    }

    template <enum_flags_c Dst, enum_c Src>
    requires is_enum_family_v<Dst, Src>
    constexpr Dst enum_family_cast(Src src) noexcept
    {
        return static_cast<Dst>(1 << std::to_underlying(src));
    }

    template <enum_c Dst, enum_flags_c Src>
    requires is_enum_family_v<Dst, Src>
    constexpr Dst enum_family_cast(Src src) noexcept
    {
        return static_cast<Dst>(std::countr_zero(static_cast<uint64_t>(std::to_underlying(src))));
    }
}

#include <boost/preprocessor.hpp>
#include <concepts/same_as_concept.h>

#define LCF_MAKE_ENUM_CATEGORY(enum_types, ...) \
    LCF_DETAIL_MAKE_ENUM_CATEGORY_IMPL( \
        BOOST_PP_TUPLE_TO_SEQ(enum_types), \
        __VA_ARGS__ \
    )

#define LCF_DETAIL_MAKE_ENUM_CATEGORY_IMPL(enum_types_seq, ...) \
    template <lcf::enum_c Enum> \
    requires lcf::any_same_as_c< \
        Enum, \
        BOOST_PP_SEQ_ENUM(enum_types_seq) \
    > \
    struct lcf::enum_category_traits<Enum> \
    { \
        using category_type = std::tuple< \
            BOOST_PP_SEQ_ENUM(enum_types_seq) \
        >; \
        constexpr static category_type mappings[] = { __VA_ARGS__ }; \
    };

/* Example usage:
#include "enum_cast.h"

namespace lib_a {
    enum class Color
    {
        eRed,
        eGreen,
        eBlue = 100,
        eYellow,
    };
}

namespace lib_b {
    enum class Color
    {
        RED,
        BLUE,
        GREEN,
    };
}

namespace lib_C {
    enum COLOR
    {
        red = 1 << 0,
        green = 1 << 1,
        blue = 1 << 2,
    };
}

LCF_MAKE_ENUM_CATEGORY((lib_a::Color, lib_b::Color, lib_C::COLOR),
    {lib_a::Color::eRed, lib_b::Color::RED, lib_C::red},
    {lib_a::Color::eGreen, lib_b::Color::GREEN, lib_C::green},
    {lib_a::Color::eBlue, lib_b::Color::BLUE, lib_C::blue}
);

// ! or without the macro:

template <lcf::enum_c Enum>
requires lcf::any_same_as_c<Enum, lib_a::Color, lib_b::Color, lib_C::COLOR>
struct lcf::enum_category_traits<Enum>
{
    using category_type = std::tuple<lib_a::Color, lib_b::Color, lib_C::COLOR>;
    constexpr static category_type mappings[] = {
        {lib_a::Color::eRed, lib_b::Color::RED, lib_C::red},
        {lib_a::Color::eGreen, lib_b::Color::GREEN, lib_C::green},
        {lib_a::Color::eBlue, lib_b::Color::BLUE, lib_C::blue},
    };
};


#include <iostream>

int main()
{
    auto c = lcf::enum_cast<lib_a::Color>(lib_b::Color::BLUE);
    std::cout << std::to_underlying(c) << std::endl;
    return 0;
}
*/