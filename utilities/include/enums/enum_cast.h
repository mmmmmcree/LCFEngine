#pragma once

#include "concepts/enum_concept.h"
#include "enum_flags.h"
#include <tuple>


namespace lcf {
    template <enum_c Dst, enum_c Src>
    struct enum_mapping_traits
    {
        using mapping_type = std::tuple<Src, Dst>;
        static constexpr size_t N = sizeof(enum_mapping_traits<Src, Dst>::mappings) / sizeof(mapping_type);
        static constexpr mapping_type mappings[N] = enum_mapping_traits<Src, Dst>::mappings;
    };

    template <enum_c Dst, enum_c Src>
    constexpr Dst enum_cast(Src src) noexcept
    {
        for (const auto & mapping : enum_mapping_traits<Dst, Src>::mappings) {
            if (std::get<Src>(mapping) == src) {
                return std::get<Dst>(mapping);
            }
        }
        return Dst {};
    }

    template <enum_flags_c Dst, enum_flags_c Src>
    constexpr Dst enum_flags_cast(Src src) noexcept
    {
        Dst dst {};
        for (const auto & mapping : enum_mapping_traits<Dst>::mappings) {
            if (static_cast<bool>(std::get<Src>(mapping) & src)) {
                dst |= std::get<Dst>(mapping);
            }
        }
        return dst;
    }
}

/* Example usage:
#include "enums/enum_cast.h"

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

template <>
struct lcf::enum_mapping_traits<lib_a::Color, lib_b::Color>
{
    constexpr static std::tuple<lib_a::Color, lib_b::Color> mappings[] = {
        {lib_a::Color::eRed, lib_b::Color::RED},
        {lib_a::Color::eGreen, lib_b::Color::GREEN},
        {lib_a::Color::eBlue, lib_b::Color::BLUE},
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