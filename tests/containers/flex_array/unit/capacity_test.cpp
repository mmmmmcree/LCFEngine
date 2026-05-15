// D. Capacity and reallocation boundaries — mirrors runCapacityEdgeTests().

#include <array/FlexArray.h>
#include <gtest/gtest.h>

namespace {

    TEST(FlexArrayCapacity, ReserveZeroOnEmpty)
    {
        lcf::FlexArray<int> a;
        a.reserve(0);
        EXPECT_EQ(a.size(), 0u);
        EXPECT_EQ(a.capacity(), 0u);
    }

    TEST(FlexArrayCapacity, ReserveBelowCurrentSizeIsNoop)
    {
        lcf::FlexArray<int> a = {1, 2, 3, 4, 5};
        const std::size_t old_cap = a.capacity();
        a.reserve(2); // n <= capacity, no-op
        EXPECT_EQ(a.size(), 5u);
        EXPECT_EQ(a.capacity(), old_cap);
    }

    TEST(FlexArrayCapacity, ReserveGrowsBeyondCurrentCapacity)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        a.reserve(100);
        EXPECT_GE(a.capacity(), 100u);
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[2], 3);
    }

    TEST(FlexArrayCapacity, ShrinkToFitFullIsNoop)
    {
        lcf::FlexArray<int> a;
        a.reserve(3);
        a.pushBack(1); a.pushBack(2); a.pushBack(3);
        const std::size_t cap_before = a.capacity();
        if (cap_before == 3u) {
            a.shrinkToFit();
            EXPECT_EQ(a.capacity(), 3u);
        }
    }

    TEST(FlexArrayCapacity, ShrinkToFitEmptyReleasesBlock)
    {
        lcf::FlexArray<int> a;
        a.reserve(100);
        EXPECT_GE(a.capacity(), 100u);
        a.shrinkToFit();
        EXPECT_EQ(a.capacity(), 0u);
    }

    TEST(FlexArrayCapacity, GrowBoundaryRepeatedExpansion)
    {
        lcf::FlexArray<int> a;
        for (int i = 0; i < 1024; ++i) {
            a.pushBack(i);
            EXPECT_GE(a.capacity(), a.size());
        }
        EXPECT_EQ(a.size(), 1024u);
        for (int i = 0; i < 1024; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)], i); }
    }

    TEST(FlexArrayCapacity, ResizeToCapacityBoundary)
    {
        lcf::FlexArray<int> a;
        a.reserve(10);
        a.resize(10, 5);
        EXPECT_EQ(a.size(), 10u);
        EXPECT_EQ(a[9], 5);
    }

} // namespace
