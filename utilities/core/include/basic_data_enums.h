#pragma once

#include <stdint.h>
#include <utility>
#include <bit>


//- BasicDataType begin
namespace lcf {
    enum BasicDataTypeCategory : uint8_t // 2 bits used
    {
        eByte,
        eInt,
        eUint,
        eFloat
    };

namespace internal {
    inline constexpr uint8_t encode_basic_data_type(BasicDataTypeCategory type, uint8_t size_in_bytes)
    {
        return std::countr_zero(size_in_bytes) << 2 | type;
    };
}

    enum class BasicDataType : uint8_t // 2 bits size | 2 bits category, 4 bits used
    {
        eByte = internal::encode_basic_data_type(BasicDataTypeCategory::eByte, 1),
        eInt8 = internal::encode_basic_data_type(BasicDataTypeCategory::eInt, 1),
        eInt16 = internal::encode_basic_data_type(BasicDataTypeCategory::eInt, 2),
        eInt32 = internal::encode_basic_data_type(BasicDataTypeCategory::eInt, 4),
        eInt64 = internal::encode_basic_data_type(BasicDataTypeCategory::eInt, 8),
        eUint8 = internal::encode_basic_data_type(BasicDataTypeCategory::eUint, 1),
        eUint16 = internal::encode_basic_data_type(BasicDataTypeCategory::eUint, 2),
        eUint32 = internal::encode_basic_data_type(BasicDataTypeCategory::eUint, 4),
        eUint64 = internal::encode_basic_data_type(BasicDataTypeCategory::eUint, 8),
        eFloat16 = internal::encode_basic_data_type(BasicDataTypeCategory::eFloat, 2),
        eFloat32 = internal::encode_basic_data_type(BasicDataTypeCategory::eFloat, 4),
        eFloat64 = internal::encode_basic_data_type(BasicDataTypeCategory::eFloat, 8)
    };
}

namespace lcf::enum_decode {
    inline constexpr BasicDataTypeCategory get_category(BasicDataType encoded)
    {
        return static_cast<BasicDataTypeCategory>(std::to_underlying(encoded) & 0b11);
    }

    inline constexpr uint32_t get_size_in_bytes(BasicDataType encoded)
    {
        return 1 << (std::to_underlying(encoded) >> 2);
    }
}

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