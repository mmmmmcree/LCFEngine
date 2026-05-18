// F. Type-specialization migration paths — tests both FlexArray and Opus_4_7.

#include <array/FlexArray.h>
#include "array/FlexArray_Opus_4_7.h"
#include <gtest/gtest.h>
#include <lcf_test/nothrow_move_only.h>
#include <lcf_test/throwing_copy.h>

namespace {

    // FlexArray paths
    TEST(FlexArraySpecialization, TriviallyCopyableUsesMigrateTrivial)
    {
        lcf::FlexArray<int> a;
        for (int i = 0; i < 50; ++i) { a.push_back(i); }
        a.reserve(1000);
        EXPECT_EQ(a.size(), 50u);
        for (int i = 0; i < 50; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)], i); }
    }

    TEST(FlexArraySpecialization, NothrowMoveOnlyUsesMigrateNothrowMove)
    {
        lcf::FlexArray<lcf::test::NothrowMoveOnly> a;
        for (int i = 0; i < 30; ++i) { a.emplace_back(i); }
        EXPECT_EQ(a.size(), 30u);
        a.reserve(500);
        EXPECT_EQ(a.size(), 30u);
        for (int i = 0; i < 30; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)].value, i); }
    }

    TEST(FlexArraySpecialization, ThrowingCopyUsesMigrateChecked)
    {
        lcf::FlexArray<lcf::test::ThrowingCopy> a;
        for (int i = 0; i < 20; ++i) { a.emplace_back(i); }
        EXPECT_EQ(a.size(), 20u);
        a.reserve(200);
        EXPECT_EQ(a.size(), 20u);
        for (int i = 0; i < 20; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)].value, i); }
    }

    // Opus_4_7 paths
    TEST(OpusSpecialization, TriviallyCopyableUsesMigrateTrivial)
    {
        lcf::FlexArray_Opus_4_7<int> a;
        for (int i = 0; i < 50; ++i) { a.push_back(i); }
        a.reserve(1000);
        EXPECT_EQ(a.size(), 50u);
        for (int i = 0; i < 50; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)], i); }
    }

    TEST(OpusSpecialization, NothrowMoveOnlyUsesMigrateNothrowMove)
    {
        lcf::FlexArray_Opus_4_7<lcf::test::NothrowMoveOnly> a;
        for (int i = 0; i < 30; ++i) { a.emplace_back(i); }
        EXPECT_EQ(a.size(), 30u);
        a.reserve(500);
        EXPECT_EQ(a.size(), 30u);
        for (int i = 0; i < 30; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)].value, i); }
    }

    TEST(OpusSpecialization, ThrowingCopyUsesMigrateChecked)
    {
        lcf::FlexArray_Opus_4_7<lcf::test::ThrowingCopy> a;
        for (int i = 0; i < 20; ++i) { a.emplace_back(i); }
        EXPECT_EQ(a.size(), 20u);
        a.reserve(200);
        EXPECT_EQ(a.size(), 20u);
        for (int i = 0; i < 20; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)].value, i); }
    }

} // namespace
