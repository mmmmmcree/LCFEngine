#pragma once

#include "basic_data_enums.h"
#include "enums/enum_value_type.h"
#include "type_traits/number_traits.h"

namespace lcf {
    template <>
    struct enum_value_type_mapping_traits<BasicDataType>
    {
        template <BasicDataType data_type, typename T>
        using make_enum_value_to_type =  enum_value_to_type<data_type, number_t<T, enum_decode::get_size_in_bytes(data_type)>>;
        using type = std::tuple<
            enum_value_to_type<BasicDataType::eByte, std::byte>,
            make_enum_value_to_type<BasicDataType::eInt8, int>,
            make_enum_value_to_type<BasicDataType::eInt16, int>,
            make_enum_value_to_type<BasicDataType::eInt32, int>,
            make_enum_value_to_type<BasicDataType::eInt64, int>,
            make_enum_value_to_type<BasicDataType::eUint8, unsigned int>,
            make_enum_value_to_type<BasicDataType::eUint16, unsigned int>,
            make_enum_value_to_type<BasicDataType::eUint32, unsigned int>,
            make_enum_value_to_type<BasicDataType::eUint64, unsigned int>,
            make_enum_value_to_type<BasicDataType::eFloat16, float>,
            make_enum_value_to_type<BasicDataType::eFloat32, float>,
            make_enum_value_to_type<BasicDataType::eFloat64, float>
        >;
    };
}