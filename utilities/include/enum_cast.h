#pragma once

#include "concepts/enum_concept.h"
#include "concepts/enum_mapping_traits_concept.h"
#include <tuple>

namespace lcf {
    template <EnumConcept Dst, EnumConcept Src>
    struct enum_category
    {
        using type = typename enum_category<Src, Dst>::type;
    };

    template <EnumConcept Dst, EnumConcept Src>
    using enum_category_t = typename enum_category<Dst, Src>::type;

    template <typename Tag>
    struct enum_mapping_traits;

    template <EnumConcept Dst, EnumConcept Src>
    constexpr Dst enum_cast(Src src)
    {
        using Category = enum_category_t<Dst, Src>;
        using MappingTraits = enum_mapping_traits<Category>;
        for (const auto& mapping : MappingTraits::mappings) {
            if (std::get<Src>(mapping) == src) {
                return std::get<Dst>(mapping);
            }
        }
        return static_cast<Dst>(0);
    }

    template <EnumConcept Dst, EnumConcept Src>
    constexpr Dst enum_flag_bits_cast(Src src)
    {
        using Category = enum_category_t<Dst, Src>;
        using MappingTraits = enum_mapping_traits<Category>;
        using SrcUnderlyingType = std::underlying_type_t<Src>;
        using DstUnderlyingType = std::underlying_type_t<Dst>;
        SrcUnderlyingType src_underlying = static_cast<SrcUnderlyingType>(src);
        DstUnderlyingType dst_underlying = 0;
        for (const auto& mapping : MappingTraits::mappings) {
            SrcUnderlyingType mask = static_cast<SrcUnderlyingType>(std::get<Src>(mapping));
            if (src_underlying & mask) {
                dst_underlying |= static_cast<DstUnderlyingType>(std::get<Dst>(mapping));
            }
        }
        return static_cast<Dst>(dst_underlying);
    }
}