// A. Construction completeness — tests FlexArray_Opus_4_7 construction edge cases.

#include <array/FlexArray.h>
#include "array/FlexArray_Opus_4_7.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <memory_resource>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

    TEST(FlexArrayConstruction, DefaultIsEmptyValid)
    {
        lcf::FlexArray_Opus_4_7<int> a;
        EXPECT_EQ(a.size(), 0u);
        EXPECT_EQ(a.capacity(), 0u);
        EXPECT_TRUE(a.empty());
        EXPECT_TRUE(a.begin() == a.end());
        EXPECT_TRUE(a.cbegin() == a.cend());
        EXPECT_TRUE(a.rbegin() == a.rend());
        EXPECT_EQ(a.data_span().size(), 0u);
        EXPECT_EQ(a.counted_bytes().size(), 0u);
    }

    TEST(FlexArrayConstruction, AllocOnlyCtor)
    {
        std::allocator<std::byte> alloc;
        lcf::FlexArray_Opus_4_7<int> a(alloc);
        EXPECT_EQ(a.size(), 0u);
        EXPECT_EQ(a.capacity(), 0u);
    }

    TEST(FlexArrayConstruction, ZeroCountFillOrDefault)
    {
        lcf::FlexArray_Opus_4_7<int> a(0, 42);
        EXPECT_EQ(a.size(), 0u);
        EXPECT_EQ(a.capacity(), 0u);
        lcf::FlexArray_Opus_4_7<int> b(static_cast<std::size_t>(0));
        EXPECT_EQ(b.size(), 0u);
        EXPECT_EQ(b.capacity(), 0u);
    }

    TEST(FlexArrayConstruction, EmptyIteratorRange)
    {
        std::vector<int> empty;
        lcf::FlexArray_Opus_4_7<int> a(empty.begin(), empty.end());
        EXPECT_EQ(a.size(), 0u);
    }

    TEST(FlexArrayConstruction, SingleElementRange)
    {
        std::vector<int> one = {7};
        lcf::FlexArray_Opus_4_7<int> a(one.begin(), one.end());
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(a[0], 7);
    }

    TEST(FlexArrayConstruction, EmptyInitializerList)
    {
        lcf::FlexArray_Opus_4_7<int> a = {};
        EXPECT_EQ(a.size(), 0u);
    }

    TEST(FlexArrayConstruction, CopyOfEmpty)
    {
        lcf::FlexArray_Opus_4_7<int> a;
        lcf::FlexArray_Opus_4_7<int> b = a;
        EXPECT_EQ(b.size(), 0u);
        EXPECT_EQ(b.capacity(), 0u);
    }

    TEST(FlexArrayConstruction, MoveOfEmpty)
    {
        lcf::FlexArray_Opus_4_7<int> a;
        lcf::FlexArray_Opus_4_7<int> b = std::move(a);
        EXPECT_EQ(b.size(), 0u);
        EXPECT_EQ(b.capacity(), 0u);
    }

    TEST(FlexArrayConstruction, CopyWithExplicitAllocator)
    {
        lcf::FlexArray_Opus_4_7<int> a = {1, 2, 3};
        std::allocator<std::byte> alloc;
        lcf::FlexArray_Opus_4_7<int> b(a, alloc);
        EXPECT_EQ(b.size(), 3u);
        EXPECT_EQ(b[2], 3);
    }

    TEST(FlexArrayConstruction, CrossAllocatorMoveDoesElementWiseMove)
    {
        using PA = std::pmr::polymorphic_allocator<std::byte>;
        std::pmr::monotonic_buffer_resource r1, r2;
        PA alloc1(&r1), alloc2(&r2);
        lcf::FlexArray_Opus_4_7<std::string, std::uint32_t, PA> a(alloc1);
        a.push_back(std::string("aa"));
        a.push_back(std::string("bb"));
        lcf::FlexArray_Opus_4_7<std::string, std::uint32_t, PA> b(std::move(a), alloc2);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b[0], std::string("aa"));
        EXPECT_EQ(b[1], std::string("bb"));
    }

    TEST(FlexArrayConstruction, FromRangeWithUnsizedFilterView)
    {
        std::vector<int> src = {1, 2, 3, 4, 5, 6};
        auto evens = src | std::views::filter([](int x) { return x % 2 == 0; });
        lcf::FlexArray_Opus_4_7<int> a(std::from_range, evens);
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[0], 2);
        EXPECT_EQ(a[2], 6);
    }

} // namespace
