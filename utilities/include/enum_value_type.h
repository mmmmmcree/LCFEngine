#pragma once

#include "concepts/enum_concept.h"
#include <tuple>

namespace lcf::detail {
    template <auto enum_value, typename Mapping>
    requires enum_c<decltype(enum_value)>
    struct enum_value_type_finder;

    template <auto enum_value, typename First, typename... Rest>
    requires enum_c<decltype(enum_value)>
    struct enum_value_type_finder<enum_value, std::tuple<First, Rest...>>
    {
        using type = std::conditional_t<
            First::value == enum_value,
            typename First::type,
            typename enum_value_type_finder<enum_value, std::tuple<Rest...>>::type
        >;
    };

    template <auto enum_value>
    requires enum_c<decltype(enum_value)>
    struct enum_value_type_finder<enum_value, std::tuple<>>
    {
        using type = void;
    };
}

namespace lcf {
    template <enum_c Enum>
    struct enum_value_type_mapping_traits;

    template <auto enum_value, typename T>
    requires enum_c<decltype(enum_value)>
    struct enum_value_to_type
    {
        static constexpr auto value = enum_value;
        using type = T;
    };

    template <auto enum_value>
    requires enum_c<decltype(enum_value)>
    using enum_value_t = typename detail::enum_value_type_finder<
        enum_value, typename enum_value_type_mapping_traits<decltype(enum_value)>::mappings>::type;
}

#include <boost/preprocessor.hpp>

#define LCF_DETAIL_MAKE_ENUM_VALUE_TO_TYPE_PAIR(r, EnumType, i, pair) \
    BOOST_PP_COMMA_IF(i) \
    lcf::enum_value_to_type< \
        EnumType::BOOST_PP_TUPLE_ELEM(0, pair), \
        BOOST_PP_TUPLE_ELEM(1, pair) \
    >

#define LCF_ENUM_VALUE_TYPE_MAPPING(EnumType, ...) \
    template <> \
    struct lcf::enum_value_type_mapping_traits<EnumType> { \
        using mappings = std::tuple< \
            BOOST_PP_SEQ_FOR_EACH_I( \
                LCF_DETAIL_MAKE_ENUM_VALUE_TO_TYPE_PAIR, \
                EnumType, \
                BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) \
            ) \
        >; \
    };

/* Example usage:
#include "enum_value_type.h"
#include <iostream>

enum class Color { eRed, eGreen, eBlue };
struct Red
{
    void print() const { std::cout << "Red" << std::endl; }
};
struct Green
{
    void print() const { std::cout << "Green" << std::endl; }
};
struct Blue
{
    void print() const { std::cout << "Blue" << std::endl; }
};

LCF_ENUM_VALUE_TYPE_MAPPING(Color,
    (eRed, Red),
    (eGreen, Green),
    (eBlue, Blue)
);

// ! or without the macro:

template <>
struct lcf::enum_value_type_mapping_traits<Color>
{
    using mappings = std::tuple<
        lcf::enum_value_to_type<Color::Red, Red>,
        lcf::enum_value_to_type<Color::Green, Green>,
        lcf::enum_value_to_type<Color::Blue, Blue>
    >;
};


template <auto enum_value>
requires lcf::enum_c<decltype(enum_value)>
void print(const lcf::enum_value_t<enum_value>& t)
{
    t.print();
}

int main()
{
    Red red;
    print<Color::eRed>(red);
}
*/