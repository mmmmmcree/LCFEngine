#pragma once

#include "math_enums.h"
#include "number_traits.h"
#include "enums/enum_value_type.h"
#include "Vector.h"

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

    template <>
    struct enum_value_type_mapping_traits<VectorType>
    {
        using type = std::tuple<
            enum_value_to_type<VectorType::e1Int8, uint8_t>,
            enum_value_to_type<VectorType::e2Int8, Vector2D<int8_t>>,
            enum_value_to_type<VectorType::e3Int8, Vector3D<int8_t>>,
            enum_value_to_type<VectorType::e4Int8, Vector4D<int8_t>>,
            enum_value_to_type<VectorType::e1Int16, uint16_t>,
            enum_value_to_type<VectorType::e2Int16, Vector2D<int16_t>>,
            enum_value_to_type<VectorType::e3Int16, Vector3D<int16_t>>,
            enum_value_to_type<VectorType::e4Int16, Vector4D<int16_t>>,
            enum_value_to_type<VectorType::e1Int32, int32_t>,
            enum_value_to_type<VectorType::e2Int32, Vector2D<int32_t>>,
            enum_value_to_type<VectorType::e3Int32, Vector3D<int32_t>>,
            enum_value_to_type<VectorType::e4Int32, Vector4D<int32_t>>,
            enum_value_to_type<VectorType::e1UInt8, uint8_t>,
            enum_value_to_type<VectorType::e2UInt8, Vector2D<uint8_t>>,
            enum_value_to_type<VectorType::e3UInt8, Vector3D<uint8_t>>,
            enum_value_to_type<VectorType::e4UInt8, Vector4D<uint8_t>>,
            enum_value_to_type<VectorType::e1UInt16, uint16_t>,
            enum_value_to_type<VectorType::e2UInt16, Vector2D<uint16_t>>,
            enum_value_to_type<VectorType::e3UInt16, Vector3D<uint16_t>>,
            enum_value_to_type<VectorType::e4UInt16, Vector4D<uint16_t>>,
            enum_value_to_type<VectorType::e1UInt32, uint32_t>,
            enum_value_to_type<VectorType::e2UInt32, Vector2D<uint32_t>>,
            enum_value_to_type<VectorType::e3UInt32, Vector3D<uint32_t>>,
            enum_value_to_type<VectorType::e4UInt32, Vector4D<uint32_t>>,
            enum_value_to_type<VectorType::e1Float16, float16_t>,
            enum_value_to_type<VectorType::e2Float16, Vector2D<float16_t>>,
            enum_value_to_type<VectorType::e3Float16, Vector3D<float16_t>>,
            enum_value_to_type<VectorType::e4Float16, Vector4D<float16_t>>,
            enum_value_to_type<VectorType::e1Float32, float>,
            enum_value_to_type<VectorType::e2Float32, Vector2D<float>>,
            enum_value_to_type<VectorType::e3Float32, Vector3D<float>>,
            enum_value_to_type<VectorType::e4Float32, Vector4D<float>>,
            enum_value_to_type<VectorType::e1Float64, double>,
            enum_value_to_type<VectorType::e2Float64, Vector2D<double>>,
            enum_value_to_type<VectorType::e3Float64, Vector3D<double>>,
            enum_value_to_type<VectorType::e4Float64, Vector4D<double>>
        >;
    };
}