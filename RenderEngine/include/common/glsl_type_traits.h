#pragma once

//- reference: http://www.opengl.org/registry/doc/glspec45.core.pdf#page=159

#include "type_traits/lcf_type_traits.h"
#include "enums/enum_value_type.h"
#include "vector_enums.h"

namespace glsl::std140 {
    //- basic types -//
    struct bool_t {};
    struct int_t {};
    struct uint_t {};
    struct float_t {};
    struct double_t {};
    //- extended basic types -//
    struct int8_t {};
    struct uint8_t {};
    struct int16_t {};
    struct uint16_t {};
    struct float16_t {};
    struct int64_t {};
    struct uint64_t {};
    //- vector types -//
    struct vec2_t {};
    struct vec3_t {};
    struct vec4_t {};
    struct dvec2_t {};
    struct dvec3_t {};
    struct dvec4_t {};
    struct ivec2_t {};
    struct ivec3_t {};
    struct ivec4_t {};
    struct uvec2_t {};
    struct uvec3_t {};
    struct uvec4_t {};
    struct bvec2_t {};
    struct bvec3_t {};
    struct bvec4_t {};
    //- extended vector types -//
    //todo
    //- matrix types -//
    //todo

    using enum_value_type_mapping_t = std::tuple<
        lcf::enum_value_to_type<lcf::BasicDataType::eInt32, int_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint32, uint_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eFloat32, float_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eFloat64, double_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eInt8, int8_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint8, uint8_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eInt16, int16_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint16, uint16_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eFloat16, float16_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eInt64, int64_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint64, uint64_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1Float32, float_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2Float32, vec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3Float32, vec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4Float32, vec4_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1Float64, double_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2Float64, dvec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3Float64, dvec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4Float64, dvec4_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1Int32, int_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2Int32, ivec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3Int32, ivec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4Int32, ivec4_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1UInt32, uint_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2UInt32, uvec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3UInt32, uvec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4UInt32, uvec4_t>
    >;
}

namespace lcf {
    //- basic types -//
    template <> inline constexpr size_t size_of_v<glsl::std140::bool_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::bool_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::int_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::int_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::uint_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uint_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::float_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::float_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::double_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::double_t> = 8;
    //- extended basic types -//
    template <> inline constexpr size_t size_of_v<glsl::std140::int8_t> = 1;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::int8_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::uint8_t> = 1;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uint8_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::int16_t> = 2;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::int16_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::uint16_t> = 2;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uint16_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::float16_t> = 2;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::float16_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std140::int64_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::int64_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std140::uint64_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uint64_t> = 8;
    //- vector types -//
    template <> inline constexpr size_t size_of_v<glsl::std140::vec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::vec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std140::vec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::vec3_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::vec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::vec4_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::dvec2_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::dvec2_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::dvec3_t> = 24;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::dvec3_t> = 32;

    template <> inline constexpr size_t size_of_v<glsl::std140::dvec4_t> = 32;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::dvec4_t> = 32;
    
    template <> inline constexpr size_t size_of_v<glsl::std140::ivec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::ivec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std140::ivec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::ivec3_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::ivec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::ivec4_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::uvec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uvec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std140::uvec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uvec3_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::uvec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::uvec4_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::bvec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::bvec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std140::bvec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::bvec3_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std140::bvec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std140::bvec4_t> = 16;

}

namespace glsl::std430 {
    //- basic types -//
    struct bool_t {};
    struct int_t {};
    struct uint_t {};
    struct float_t {};
    struct double_t {};
    //- extended basic types -//
    struct int8_t {};
    struct uint8_t {};
    struct int16_t {};
    struct uint16_t {};
    struct float16_t {};
    struct int64_t {};
    struct uint64_t {};
    //- vector types -//
    struct vec2_t {};
    struct vec3_t {};
    struct vec4_t {};
    struct dvec2_t {};
    struct dvec3_t {};
    struct dvec4_t {};
    struct ivec2_t {};
    struct ivec3_t {};
    struct ivec4_t {};
    struct uvec2_t {};
    struct uvec3_t {};
    struct uvec4_t {};
    struct bvec2_t {};
    struct bvec3_t {};
    struct bvec4_t {};
    //- extended vector types -//
    //todo
    //- matrix types -//
    //todo

    //- enum value types -//
    using enum_value_type_mapping_t = std::tuple<
        lcf::enum_value_to_type<lcf::BasicDataType::eInt32, int_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint32, uint_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eFloat32, float_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eFloat64, double_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eInt8, int8_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint8, uint8_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eInt16, int16_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint16, uint16_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eFloat16, float16_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eInt64, int64_t>,
        lcf::enum_value_to_type<lcf::BasicDataType::eUint64, uint64_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1Float32, float_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2Float32, vec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3Float32, vec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4Float32, vec4_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1Float64, double_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2Float64, dvec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3Float64, dvec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4Float64, dvec4_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1Int32, int_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2Int32, ivec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3Int32, ivec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4Int32, ivec4_t>,
        lcf::enum_value_to_type<lcf::VectorType::e1UInt32, uint_t>,
        lcf::enum_value_to_type<lcf::VectorType::e2UInt32, uvec2_t>,
        lcf::enum_value_to_type<lcf::VectorType::e3UInt32, uvec3_t>,
        lcf::enum_value_to_type<lcf::VectorType::e4UInt32, uvec4_t>
    >;
}

namespace lcf {
    //- basic types -//
    template <> inline constexpr size_t size_of_v<glsl::std430::bool_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::bool_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std430::int_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::int_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std430::uint_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uint_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std430::float_t> = 4;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::float_t> = 4;

    template <> inline constexpr size_t size_of_v<glsl::std430::double_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::double_t> = 8;
    //- extended basic types -//
    template <> inline constexpr size_t size_of_v<glsl::std430::int8_t> = 1;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::int8_t> = 1;

    template <> inline constexpr size_t size_of_v<glsl::std430::uint8_t> = 1;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uint8_t> = 1;

    template <> inline constexpr size_t size_of_v<glsl::std430::int16_t> = 2;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::int16_t> = 2;

    template <> inline constexpr size_t size_of_v<glsl::std430::uint16_t> = 2;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uint16_t> = 2;

    template <> inline constexpr size_t size_of_v<glsl::std430::float16_t> = 2;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::float16_t> = 2;

    template <> inline constexpr size_t size_of_v<glsl::std430::int64_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::int64_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std430::uint64_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uint64_t> = 8;
    //- vector types -//
    template <> inline constexpr size_t size_of_v<glsl::std430::vec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::vec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std430::vec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::vec3_t> = 12;

    template <> inline constexpr size_t size_of_v<glsl::std430::vec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::vec4_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std430::dvec2_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::dvec2_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std430::dvec3_t> = 24;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::dvec3_t> = 24;

    template <> inline constexpr size_t size_of_v<glsl::std430::dvec4_t> = 32;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::dvec4_t> = 32;
    
    template <> inline constexpr size_t size_of_v<glsl::std430::ivec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::ivec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std430::ivec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::ivec3_t> = 12;

    template <> inline constexpr size_t size_of_v<glsl::std430::ivec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::ivec4_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std430::uvec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uvec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std430::uvec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uvec3_t> = 12;

    template <> inline constexpr size_t size_of_v<glsl::std430::uvec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::uvec4_t> = 16;

    template <> inline constexpr size_t size_of_v<glsl::std430::bvec2_t> = 8;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::bvec2_t> = 8;

    template <> inline constexpr size_t size_of_v<glsl::std430::bvec3_t> = 12;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::bvec3_t> = 12;

    template <> inline constexpr size_t size_of_v<glsl::std430::bvec4_t> = 16;
    template <> inline constexpr size_t alignment_of_v<glsl::std430::bvec4_t> = 16;

}
