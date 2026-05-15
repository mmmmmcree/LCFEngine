// F. Type-specialization migration paths — mirrors runSpecializationPathTests().

#include <array/FlexArray.h>
#include <gtest/gtest.h>
#include <lcf_test/nothrow_move_only.h>
#include <lcf_test/throwing_copy.h>

namespace {

    TEST(FlexArraySpecialization, TriviallyCopyableUsesMigrateTrivial)
    {
        lcf::FlexArray<int> a;
        for (int i = 0; i < 50; ++i) { a.pushBack(i); }
        a.reserve(1000); // force migration -> memcpy path
        EXPECT_EQ(a.size(), 50u);
        for (int i = 0; i < 50; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)], i); }
    }

    TEST(FlexArraySpecialization, NothrowMoveOnlyUsesMigrateNothrowMove)
    {
        lcf::FlexArray<lcf::test::NothrowMoveOnly> a;
        for (int i = 0; i < 30; ++i) { a.emplaceBack(i); }
        EXPECT_EQ(a.size(), 30u);
        a.reserve(500); // force migration -> migrate_nothrow_move path
        EXPECT_EQ(a.size(), 30u);
        for (int i = 0; i < 30; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)].value, i); }
    }

    TEST(FlexArraySpecialization, ThrowingCopyUsesMigrateChecked)
    {
        lcf::FlexArray<lcf::test::ThrowingCopy> a;
        for (int i = 0; i < 20; ++i) { a.emplaceBack(i); }
        EXPECT_EQ(a.size(), 20u);
        a.reserve(200); // force migration -> migrate_checked path
        EXPECT_EQ(a.size(), 20u);
        for (int i = 0; i < 20; ++i) { EXPECT_EQ(a[static_cast<std::size_t>(i)].value, i); }
    }

} // namespace
