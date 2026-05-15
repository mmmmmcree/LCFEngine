// C. Modifier edge cases — mirrors runModifierEdgeTests().

#include <array/FlexArray.h>
#include <gtest/gtest.h>

#include <list>
#include <ranges>
#include <vector>

namespace {

    TEST(FlexArrayModifiers, PopBackOnEmptyIsNoop)
    {
        lcf::FlexArray<int> a;
        a.popBack();
        EXPECT_EQ(a.size(), 0u);
    }

    TEST(FlexArrayModifiers, EmplaceAtBegin)
    {
        lcf::FlexArray<int> a = {2, 3, 4};
        auto it = a.emplace(a.cbegin(), 1);
        EXPECT_EQ(a.size(), 4u);
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(*it, 1);
    }

    TEST(FlexArrayModifiers, EmplaceAtEnd)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        auto it = a.emplace(a.cend(), 4);
        EXPECT_EQ(a.size(), 4u);
        EXPECT_EQ(a[3], 4);
        EXPECT_EQ(*it, 4);
    }

    TEST(FlexArrayModifiers, EraseEmptyRangeIsNoop)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        auto it = a.erase(a.cbegin() + 1, a.cbegin() + 1);
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(*it, 2);
    }

    TEST(FlexArrayModifiers, EraseAllElements)
    {
        lcf::FlexArray<int> a = {1, 2, 3, 4};
        a.erase(a.cbegin(), a.cend());
        EXPECT_EQ(a.size(), 0u);
        EXPECT_TRUE(a.isEmpty());
    }

    TEST(FlexArrayModifiers, AssignZeroCount)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        a.assign(0, 99);
        EXPECT_EQ(a.size(), 0u);
    }

    TEST(FlexArrayModifiers, AssignCountBeyondCapacityTriggersRealloc)
    {
        lcf::FlexArray<int> a = {1};
        a.assign(100, 7);
        EXPECT_EQ(a.size(), 100u);
        EXPECT_EQ(a[0], 7);
        EXPECT_EQ(a[99], 7);
    }

    TEST(FlexArrayModifiers, AssignIteratorsEmptyRange)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        std::vector<int> empty;
        a.assign(empty.begin(), empty.end());
        EXPECT_EQ(a.size(), 0u);
    }

    TEST(FlexArrayModifiers, AssignInputIteratorViaStdList)
    {
        lcf::FlexArray<int> a;
        std::list<int> src = {10, 20, 30};
        a.assign(src.begin(), src.end());
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[2], 30);
    }

    TEST(FlexArrayModifiers, AssignInitializerList)
    {
        lcf::FlexArray<int> a;
        a.assign({5, 6, 7});
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[1], 6);
    }

    TEST(FlexArrayModifiers, InsertRangeAtHead)
    {
        lcf::FlexArray<int> a = {10, 20, 30};
        auto it = a.insertRange(a.cbegin(), std::views::iota(1, 4));
        EXPECT_EQ(a.size(), 6u);
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a[2], 3);
        EXPECT_EQ(a[3], 10);
        EXPECT_EQ(*it, 1);
    }

    TEST(FlexArrayModifiers, InsertRangeAtMiddle)
    {
        lcf::FlexArray<int> a = {1, 2, 5, 6};
        a.insertRange(a.cbegin() + 2, std::views::iota(3, 5));
        EXPECT_EQ(a.size(), 6u);
        EXPECT_EQ(a[2], 3);
        EXPECT_EQ(a[3], 4);
        EXPECT_EQ(a[5], 6);
    }

    TEST(FlexArrayModifiers, InsertRangeAtTail)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        a.insertRange(a.cend(), std::views::iota(4, 6));
        EXPECT_EQ(a.size(), 5u);
        EXPECT_EQ(a[4], 5);
    }

    TEST(FlexArrayModifiers, InsertRangeEmpty)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        a.insertRange(a.cbegin() + 1, std::views::iota(0, 0));
        EXPECT_EQ(a.size(), 3u);
    }

    TEST(FlexArrayModifiers, InsertRangeUnsizedFilterView)
    {
        lcf::FlexArray<int> a = {1, 2};
        std::vector<int> src = {10, 11, 12, 13};
        auto odd = src | std::views::filter([](int x) { return x % 2 == 1; });
        a.insertRange(a.cend(), odd);
        EXPECT_EQ(a.size(), 4u);
        EXPECT_EQ(a[2], 11);
        EXPECT_EQ(a[3], 13);
    }

} // namespace
