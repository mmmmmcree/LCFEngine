#pragma once

#include "concepts/enum_concept.h"

namespace lcf {
    template <auto enum_value, typename Type>
    requires enum_c<decltype(enum_value)>
    struct enum_value_to_type {
        static constexpr auto value = enum_value;
        using type = Type;
    };

    template <typename T>
    struct is_enum_value_to_type : std::false_type {};

    template <auto enum_value, typename Type>
    struct is_enum_value_to_type<enum_value_to_type<enum_value, Type>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_enum_value_to_type_v = is_enum_value_to_type<T>::value;

    template <typename T>
    concept enum_value_to_type_c = is_enum_value_to_type_v<T>;
}

namespace lcf::detail {
    template <auto enum_value, typename Mapping>
    requires enum_c<decltype(enum_value)>
    struct enum_value_type_finder;

    template <auto enum_value, 
        template<enum_value_to_type_c...> typename Mapping,
        enum_value_to_type_c First, 
        enum_value_to_type_c... Rest>
    requires enum_c<decltype(enum_value)>
    struct enum_value_type_finder<enum_value, Mapping<First, Rest...>>
    {
        using type = std::conditional_t<
            First::value == enum_value,
            typename First::type,
            typename enum_value_type_finder<enum_value, Mapping<Rest...>>::type
        >;
    };

    template <auto enum_value, template<enum_value_to_type_c...> typename Mapping>
    requires enum_c<decltype(enum_value)>
    struct enum_value_type_finder<enum_value, Mapping<>>
    {
        using type = void;
    };
}

#include <tuple>

namespace lcf {
    template <enum_c Enum>
    struct enum_value_type_mapping_traits
    {
        using type = std::tuple<>;
    };

    template <auto enum_value, typename Mapping = typename enum_value_type_mapping_traits<decltype(enum_value)>::type>
    requires enum_c<decltype(enum_value)>
    using enum_value_t = typename detail::enum_value_type_finder<enum_value, Mapping>::type;
}

/* Example Usage:
#include "enum_value_type.h"
#include <tuple>

enum class Color { Red, Green, Blue };
enum class DataType { Int8, Int16, Int32, Float, Double };

using ColorMapping = std::tuple<
    lcf::enum_value_to_type<Color::Red, int>,
    lcf::enum_value_to_type<Color::Green, double>,
    lcf::enum_value_to_type<Color::Blue, float>
>;

using DataTypeMapping = std::tuple<
    lcf::enum_value_to_type<DataType::Int8, int8_t>,
    lcf::enum_value_to_type<DataType::Int16, int16_t>,
    lcf::enum_value_to_type<DataType::Int32, int32_t>,
    lcf::enum_value_to_type<DataType::Float, float>,
    lcf::enum_value_to_type<DataType::Double, double>
>;

template <>
struct lcf::enum_value_type_mapping_traits<DataType>
{
    using type = DataTypeMapping;
};

int main() {
    static_assert(std::is_same_v<lcf::enum_value_t<Color::Red, ColorMapping>, int>);
    static_assert(std::is_same_v<lcf::enum_value_t<DataType::Int8, DataTypeMapping>, int8_t>);
    static_assert(std::is_same_v<lcf::enum_value_t<DataType::Int8>, int8_t>);
    return 0;
}
*/