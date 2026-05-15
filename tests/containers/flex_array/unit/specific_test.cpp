// FlexArray-specific behaviors that don't fit the generic Vector contract.
// Mirrors runFlexArraySpecificTests().

#include <array/FlexArray.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <list>
#include <memory_resource>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

    TEST(FlexArraySpecific, GetCountedBytesUint32)
    {
        static_assert(sizeof(lcf::FlexArray<int, std::uint32_t>::size_type) == sizeof(std::size_t));
        lcf::FlexArray<int, std::uint32_t> a;
        for (int i = 0; i < 10; ++i) { a.pushBack(i); }
        const auto bytes = a.getCountedBytes();
        const std::size_t expected = 2 * sizeof(std::uint32_t) + 10 * sizeof(int);
        EXPECT_EQ(bytes.size(), expected);

        const auto * size_ptr = reinterpret_cast<const std::uint32_t *>(bytes.data());
        EXPECT_EQ(*size_ptr, 10u);
        const auto * cap_ptr = size_ptr + 1;
        EXPECT_GE(*cap_ptr, 10u);
    }

    TEST(FlexArraySpecific, GetCountedBytesUint16)
    {
        lcf::FlexArray<int, std::uint16_t> a;
        for (int i = 0; i < 5; ++i) { a.pushBack(i); }
        const auto bytes = a.getCountedBytes();
        const std::size_t expected = 2 * sizeof(std::uint16_t) + 5 * sizeof(int);
        EXPECT_EQ(bytes.size(), expected);
    }

    TEST(FlexArraySpecific, GetCountedBytesUint64WithDouble)
    {
        lcf::FlexArray<double, std::uint64_t> a;
        for (int i = 0; i < 3; ++i) { a.pushBack(static_cast<double>(i) + 0.5); }
        const auto bytes = a.getCountedBytes();
        const std::size_t expected = 2 * sizeof(std::uint64_t) + 3 * sizeof(double);
        EXPECT_EQ(bytes.size(), expected);
    }

    TEST(FlexArraySpecific, ClearAndShrinkReleasesBlock)
    {
        lcf::FlexArray<int> a;
        for (int i = 0; i < 32; ++i) { a.pushBack(i); }
        EXPECT_EQ(a.size(), 32u);
        EXPECT_GE(a.capacity(), 32u);
        a.clearAndShrink();
        EXPECT_EQ(a.size(), 0u);
        EXPECT_EQ(a.capacity(), 0u);
        EXPECT_TRUE(a.isEmpty());
    }

    TEST(FlexArraySpecific, NonTrivialElementCopyAndMove)
    {
        lcf::FlexArray<std::string> a;
        a.pushBack(std::string("hello"));
        a.emplaceBack("world");
        EXPECT_EQ(a.size(), 2u);
        EXPECT_EQ(a[0], std::string("hello"));
        EXPECT_EQ(a[1], std::string("world"));

        lcf::FlexArray<std::string> b = a;
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b[0], std::string("hello"));

        lcf::FlexArray<std::string> c = std::move(b);
        EXPECT_EQ(c.size(), 2u);
        EXPECT_EQ(c[1], std::string("world"));
    }

    TEST(FlexArraySpecific, ResizeAndShrinkToFit)
    {
        lcf::FlexArray<int> a = {1, 2, 3, 4, 5};
        EXPECT_EQ(a.size(), 5u);
        a.resize(8, 42);
        EXPECT_EQ(a.size(), 8u);
        EXPECT_EQ(a[5], 42);
        EXPECT_EQ(a[7], 42);
        a.resize(2);
        EXPECT_EQ(a.size(), 2u);
        EXPECT_EQ(a[0], 1);

        a.shrinkToFit();
        EXPECT_EQ(a.capacity(), 2u);
    }

    TEST(FlexArraySpecific, AtThrowsOutOfRangeOnEmpty)
    {
        lcf::FlexArray<int> a;
        EXPECT_THROW((void)a.at(0), std::out_of_range);
    }

    TEST(FlexArraySpecific, RangeApi)
    {
        lcf::FlexArray<int> a;
        auto r = std::views::iota(0, 10);
        a.assignRange(r);
        EXPECT_EQ(a.size(), 10u);
        EXPECT_EQ(a[0], 0);
        EXPECT_EQ(a[9], 9);

        a.appendRange(std::views::iota(10, 15));
        EXPECT_EQ(a.size(), 15u);
        EXPECT_EQ(a[14], 14);

        lcf::FlexArray<int> b(std::from_range, std::views::iota(100, 105));
        EXPECT_EQ(b.size(), 5u);
        EXPECT_EQ(b[0], 100);
        EXPECT_EQ(b[4], 104);
    }

    TEST(FlexArraySpecific, PmrPolymorphicAllocatorBasic)
    {
        std::pmr::monotonic_buffer_resource resource;
        std::pmr::polymorphic_allocator<std::byte> alloc(&resource);
        lcf::FlexArray<int, std::uint32_t, std::pmr::polymorphic_allocator<std::byte>> a(alloc);
        for (int i = 0; i < 50; ++i) { a.pushBack(i); }
        EXPECT_EQ(a.size(), 50u);
        for (int i = 0; i < 50; ++i) {
            EXPECT_EQ(a[static_cast<std::size_t>(i)], i);
        }
    }

    TEST(FlexArraySpecific, TryReserveCapacityOverflow)
    {
        lcf::FlexArray<int> a;
        auto result = a.tryReserve(static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1);
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), std::make_error_code(std::errc::value_too_large));
    }

    TEST(FlexArraySpecific, TryEmplaceBackSuccess)
    {
        lcf::FlexArray<int> a = {10, 20, 30};
        auto result = a.tryEmplaceBack(40);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result->get(), 40);
        EXPECT_EQ(a.size(), 4u);
        EXPECT_EQ(a[3], 40);
    }

    TEST(FlexArraySpecific, InsertAndErase)
    {
        lcf::FlexArray<int> a = {1, 2, 3, 4, 5};
        a.insert(a.cbegin() + 2, 99);
        EXPECT_EQ(a.size(), 6u);
        EXPECT_EQ(a[2], 99);
        EXPECT_EQ(a[5], 5);

        a.erase(a.cbegin() + 2);
        EXPECT_EQ(a.size(), 5u);
        EXPECT_EQ(a[2], 3);

        a.erase(a.cbegin() + 1, a.cbegin() + 3);
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a[1], 4);
        EXPECT_EQ(a[2], 5);
    }

    TEST(FlexArraySpecific, DataSpanAndReverseIteration)
    {
        lcf::FlexArray<int> a = {1, 2, 3};
        const auto data_span = a.getDataSpan();
        EXPECT_EQ(data_span.size(), 3u);
        EXPECT_EQ(data_span[0], 1);
        EXPECT_EQ(data_span[2], 3);

        const lcf::FlexArray<int> & cref = a;
        const auto cspan = cref.getDataSpan();
        static_assert(std::is_same_v<decltype(cspan), const std::span<const int>>);
        EXPECT_EQ(cspan.size(), 3u);

        int sum = 0;
        for (auto rit = a.rbegin(); rit != a.rend(); ++rit) {
            sum = sum * 10 + *rit;
        }
        EXPECT_EQ(sum, 321);
    }

    TEST(FlexArraySpecific, ConstructFromInputIteratorList)
    {
        std::list<int> list_data = {1, 2, 3, 4, 5};
        lcf::FlexArray<int> a(list_data.begin(), list_data.end());
        EXPECT_EQ(a.size(), 5u);
        EXPECT_EQ(a[0], 1);
        EXPECT_EQ(a[4], 5);
    }

} // namespace
