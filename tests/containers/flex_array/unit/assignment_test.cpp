// B. Assignment-operator edge cases — mirrors runAssignmentEdgeTests().

#include <array/FlexArray.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory_resource>
#include <string>
#include <utility>

namespace {

    TEST(FlexArrayAssignment, SelfCopyAssignment)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        lcf::FlexArray<int> & ref = a;
        a = ref;
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a[2], 3);
    }

    TEST(FlexArrayAssignment, SelfMoveAssignment)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        lcf::FlexArray<int> & ref = a;
        a = std::move(ref);
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[2], 3);
    }

    TEST(FlexArrayAssignment, InitializerListAssignment)
    {
        lcf::FlexArray<int> a = {1, 2};
        a = {10, 20, 30, 40};
        EXPECT_EQ(a.size(), 4u);
        EXPECT_EQ(a[0], 10);
        EXPECT_EQ(a[3], 40);
    }

    TEST(FlexArrayAssignment, CopyAssignFromEmpty)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        lcf::FlexArray<int> empty;
        a = empty;
        EXPECT_EQ(a.size(), 0u);
    }

    TEST(FlexArrayAssignment, CopyAssignToEmpty)
    {
        lcf::FlexArray<int> empty;
        lcf::FlexArray<int> src = {1, 2, 3};
        empty = src;
        EXPECT_EQ(empty.size(), 3u);
        EXPECT_EQ(empty[2], 3);
    }

    TEST(FlexArrayAssignment, CrossAllocatorPmrMoveAssignDoesElementWiseMove)
    {
        using PA = std::pmr::polymorphic_allocator<std::byte>;
        std::pmr::monotonic_buffer_resource r1, r2;
        PA alloc1(&r1), alloc2(&r2);
        lcf::FlexArray<std::string, std::uint32_t, PA> a(alloc1);
        a.pushBack(std::string("hello"));
        a.pushBack(std::string("world"));
        lcf::FlexArray<std::string, std::uint32_t, PA> b(alloc2);
        b = std::move(a);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b[0], std::string("hello"));
        EXPECT_EQ(b[1], std::string("world"));
    }

} // namespace
