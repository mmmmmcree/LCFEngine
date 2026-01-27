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

//- BasicDataType end

//- VectorType begin
namespace lcf::internal {
    inline constexpr uint8_t encode_vector_type(BasicDataType type, uint8_t vec_dimension) noexcept
    {
        return (vec_dimension - 1) << 4 | std::to_underlying(type);
    }
}

namespace lcf {
    enum class VectorType : uint8_t // 2 bits for vec_dimension | 4 bits for basic data type, 6 bits used
    {
        e1Int8 = internal::encode_vector_type(BasicDataType::eInt8, 1),
        e2Int8 = internal::encode_vector_type(BasicDataType::eInt8, 2),
        e3Int8 = internal::encode_vector_type(BasicDataType::eInt8, 3),
        e4Int8 = internal::encode_vector_type(BasicDataType::eInt8, 4),
        e1Int16 = internal::encode_vector_type(BasicDataType::eInt16, 1),
        e2Int16 = internal::encode_vector_type(BasicDataType::eInt16, 2),
        e3Int16 = internal::encode_vector_type(BasicDataType::eInt16, 3),
        e4Int16 = internal::encode_vector_type(BasicDataType::eInt16, 4),
        e1Int32 = internal::encode_vector_type(BasicDataType::eInt32, 1),
        e2Int32 = internal::encode_vector_type(BasicDataType::eInt32, 2),
        e3Int32 = internal::encode_vector_type(BasicDataType::eInt32, 3),
        e4Int32 = internal::encode_vector_type(BasicDataType::eInt32, 4),
        e1UInt8 = internal::encode_vector_type(BasicDataType::eUint8, 1),
        e2UInt8 = internal::encode_vector_type(BasicDataType::eUint8, 2),
        e3UInt8 = internal::encode_vector_type(BasicDataType::eUint8, 3),
        e4UInt8 = internal::encode_vector_type(BasicDataType::eUint8, 4),
        e1UInt16 = internal::encode_vector_type(BasicDataType::eUint16, 1),
        e2UInt16 = internal::encode_vector_type(BasicDataType::eUint16, 2),
        e3UInt16 = internal::encode_vector_type(BasicDataType::eUint16, 3),
        e4UInt16 = internal::encode_vector_type(BasicDataType::eUint16, 4),
        e1UInt32 = internal::encode_vector_type(BasicDataType::eUint32, 1),
        e2UInt32 = internal::encode_vector_type(BasicDataType::eUint32, 2),
        e3UInt32 = internal::encode_vector_type(BasicDataType::eUint32, 3),
        e4UInt32 = internal::encode_vector_type(BasicDataType::eUint32, 4),
        e1Float16 = internal::encode_vector_type(BasicDataType::eFloat16, 1),
        e2Float16 = internal::encode_vector_type(BasicDataType::eFloat16, 2),
        e3Float16 = internal::encode_vector_type(BasicDataType::eFloat16, 3),
        e4Float16 = internal::encode_vector_type(BasicDataType::eFloat16, 4),
        e1Float32 = internal::encode_vector_type(BasicDataType::eFloat32, 1),
        e2Float32 = internal::encode_vector_type(BasicDataType::eFloat32, 2),
        e3Float32 = internal::encode_vector_type(BasicDataType::eFloat32, 3),
        e4Float32 = internal::encode_vector_type(BasicDataType::eFloat32, 4),
        e1Float64 = internal::encode_vector_type(BasicDataType::eFloat64, 1),
        e2Float64 = internal::encode_vector_type(BasicDataType::eFloat64, 2),
        e3Float64 = internal::encode_vector_type(BasicDataType::eFloat64, 3),
        e4Float64 = internal::encode_vector_type(BasicDataType::eFloat64, 4)
    };
}

namespace lcf::enum_decode {
    inline constexpr BasicDataType get_basic_data_type(VectorType encoded) noexcept
    {
        return static_cast<BasicDataType>(std::to_underlying(encoded) & 0xF);
    }

    inline constexpr BasicDataTypeCategory get_category(VectorType encoded) noexcept
    {
        return get_category(get_basic_data_type(encoded));
    }

    inline constexpr uint32_t get_vector_dimension(VectorType encoded) noexcept
    {
        return (std::to_underlying(encoded) >> 4) + 1;
    }

    inline constexpr uint32_t get_size_in_bytes(VectorType encoded) noexcept
    {
        return get_size_in_bytes(get_basic_data_type(encoded)) * get_vector_dimension(encoded);
    }
}
//- VectorType end