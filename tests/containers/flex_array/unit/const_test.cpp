// G. const correctness — mirrors runConstCorrectnessTests().

#include <array/FlexArray.h>
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
        EXPECT_FALSE(a.isEmpty());
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a.at(4), 5);
        EXPECT_EQ(a.front(), 1);
        EXPECT_EQ(a.back(), 5);

        auto span = a.getDataSpan();
        static_assert(std::is_same_v<decltype(span)::element_type, const int>);
        EXPECT_EQ(span.size(), 5u);

        EXPECT_TRUE(a.begin() == a.cbegin());
        EXPECT_TRUE(a.end() == a.cend());
        EXPECT_TRUE(a.rbegin() == a.crbegin());
        EXPECT_TRUE(a.rend() == a.crend());

        int sum = 0;
        for (int v : a) { sum += v; }
        EXPECT_EQ(sum, 15);

        EXPECT_EQ(a.getCountedBytes().size(), 2u * sizeof(std::uint32_t) + 5u * sizeof(int));
        EXPECT_GE(a.getCountedBytesWithCapacity().size(), a.getCountedBytes().size());

        (void)a.get_allocator();
    }

    TEST(FlexArrayConst, TryReserveSuccess)
    {
        lcf::FlexArray<int> b;
        auto r = b.tryReserve(8);
        EXPECT_TRUE(r.has_value());
        EXPECT_GE(b.capacity(), 8u);
    }

} // namespace
