// G. const correctness — tests both FlexArray and FlexArray_Opus_4_7 (snake_case).

#include <array/FlexArray.h>
#include "array/FlexArray_Opus_4_7.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <type_traits>

namespace {

    TEST(FlexArrayConst, AllAccessorsCallableOnConstRef)
    {
        lcf::FlexArray<int> mutable_a = {1, 2, 3, 4, 5};
        const lcf::FlexArray<int> & a = mutable_a;

        EXPECT_EQ(a.size(), 5u);
        EXPECT_GE(a.capacity(), 5u);
        EXPECT_FALSE(a.empty());
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a.front(), 1);
        EXPECT_EQ(a.back(), 5);

        auto span = a.data_span();
        static_assert(std::is_same_v<decltype(span)::element_type, const int>);
        EXPECT_EQ(span.size(), 5u);

        EXPECT_EQ(a.begin(), a.cbegin());
        EXPECT_EQ(a.end(), a.cend());
        EXPECT_EQ(a.rbegin(), a.crbegin());
        EXPECT_EQ(a.rend(), a.crend());

        int sum = 0;
        for (int v : a) { sum += v; }
        EXPECT_EQ(sum, 15);

        auto bytes = a.counted_bytes();
        EXPECT_GT(bytes.size(), 0u);

        (void)a.get_allocator();
    }

    TEST(OpusConst, AllAccessorsCallableOnConstRef)
    {
        lcf::FlexArray_Opus_4_7<int> mutable_a = {1, 2, 3, 4, 5};
        const lcf::FlexArray_Opus_4_7<int> & a = mutable_a;

        EXPECT_EQ(a.size(), 5u);
        EXPECT_GE(a.capacity(), 5u);
        EXPECT_FALSE(a.empty());
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a.at(4), 5);
        EXPECT_EQ(a.front(), 1);
        EXPECT_EQ(a.back(), 5);

        auto span = a.data_span();
        static_assert(std::is_same_v<decltype(span)::element_type, const int>);
        EXPECT_EQ(span.size(), 5u);

        EXPECT_EQ(a.begin(), a.cbegin());
        EXPECT_EQ(a.end(), a.cend());
        EXPECT_EQ(a.rbegin(), a.crbegin());
        EXPECT_EQ(a.rend(), a.crend());

        int sum = 0;
        for (int v : a) { sum += v; }
        EXPECT_EQ(sum, 15);

        auto bytes = a.counted_bytes();
        EXPECT_EQ(bytes.size(), 2u * sizeof(std::uint32_t) + 5u * sizeof(int));
        auto bytes_cap = a.counted_bytes_with_capacity();
        EXPECT_GE(bytes_cap.size(), bytes.size());

        (void)a.get_allocator();
    }

    TEST(OpusConst, TryReserveSuccess)
    {
        lcf::FlexArray_Opus_4_7<int> b;
        auto r = b.try_reserve(8);
        ASSERT_TRUE(r.has_value());
        EXPECT_GE(b.capacity(), 8u);
    }

} // namespace
