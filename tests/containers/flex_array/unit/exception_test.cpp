// E. Exception safety — mirrors runExceptionSafetyTests().

#include <array/FlexArray.h>
#include <gtest/gtest.h>
#include <lcf_test/throw_on_nth.h>
#include <lcf_test/counting_allocator.h>

#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {

    using lcf::test::ThrowOnNth;

    TEST(FlexArrayException, FillCtorThirdElementThrows)
    {
        const std::size_t baseline = ThrowOnNth::s_alive_count.load();
        ThrowOnNth::reset(0);
        {
            ThrowOnNth proto(7);                               // alive +1
            ThrowOnNth::reset(/*throw_at=*/3);                 // re-arm trigger
            EXPECT_THROW({
                lcf::FlexArray_Opus_4_7<ThrowOnNth> a(5, proto);
                (void)a;
            }, std::runtime_error);
        }
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), baseline);
        ThrowOnNth::reset(0);
    }

    TEST(FlexArrayException, DefaultFillCtorFourthThrows)
    {
        const std::size_t baseline = ThrowOnNth::s_alive_count.load();
        ThrowOnNth::reset(/*throw_at=*/4);
        EXPECT_THROW({
            lcf::FlexArray_Opus_4_7<ThrowOnNth> a(10);
            (void)a;
        }, std::runtime_error);
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), baseline);
        ThrowOnNth::reset(0);
    }

    TEST(FlexArrayException, IteratorRangeCtorThirdCopyThrows)
    {
        ThrowOnNth::reset(0);
        std::vector<ThrowOnNth> seeds;
        seeds.reserve(5);
        for (int v : {10, 20, 30, 40, 50}) { seeds.emplace_back(v); }
        const std::size_t baseline = ThrowOnNth::s_alive_count.load();

        ThrowOnNth::reset(/*throw_at=*/3);
        EXPECT_THROW({
            lcf::FlexArray_Opus_4_7<ThrowOnNth> a(seeds.begin(), seeds.end());
            (void)a;
        }, std::runtime_error);
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), baseline);
        ThrowOnNth::reset(0);
    }

    TEST(FlexArrayException, CopyCtorWithThrowingElement)
    {
        ThrowOnNth::reset(0);
        lcf::FlexArray_Opus_4_7<ThrowOnNth> src;
        for (int v : {1, 2, 3, 4}) { src.emplaceBack(v); }
        const std::size_t baseline = ThrowOnNth::s_alive_count.load();

        ThrowOnNth::reset(/*throw_at=*/3);
        EXPECT_THROW({
            lcf::FlexArray_Opus_4_7<ThrowOnNth> copy = src;
            (void)copy;
        }, std::runtime_error);
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), baseline);
        EXPECT_EQ(src.size(), 4u);
        ThrowOnNth::reset(0);
    }

    TEST(FlexArrayException, EmplaceBackThrowPreservesSize)
    {
        ThrowOnNth::reset(0);
        lcf::FlexArray_Opus_4_7<ThrowOnNth> a;
        for (int v : {1, 2, 3}) { a.emplaceBack(v); }
        const std::size_t size_before = a.size();
        const std::size_t alive_before = ThrowOnNth::s_alive_count.load();

        ThrowOnNth::reset(/*throw_at=*/1);
        EXPECT_THROW(a.emplaceBack(99), std::runtime_error);
        EXPECT_EQ(a.size(), size_before);
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), alive_before);
        ThrowOnNth::reset(0);
    }

    TEST(FlexArrayException, TryEmplaceBackReturnsUnexpectedOnThrow)
    {
        ThrowOnNth::reset(0);
        lcf::FlexArray_Opus_4_7<ThrowOnNth> a;
        a.emplaceBack(1);
        const std::size_t size_before = a.size();
        const std::size_t alive_before = ThrowOnNth::s_alive_count.load();

        ThrowOnNth::reset(/*throw_at=*/1);
        auto result = a.tryEmplaceBack(42);
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), std::make_error_code(std::errc::operation_canceled));
        EXPECT_EQ(a.size(), size_before);
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), alive_before);
        ThrowOnNth::reset(0);
    }

    TEST(FlexArrayException, CountingAllocatorMatchedAllocDealloc)
    {
        lcf::test::AllocStats stats;
        using A = lcf::test::CountingAllocator<std::byte>;
        {
            lcf::FlexArray_Opus_4_7<int, std::uint32_t, A> v(A{&stats});
            for (int i = 0; i < 100; ++i) { v.pushBack(i); }
            EXPECT_EQ(v.size(), 100u);
            EXPECT_GE(stats.alloc_calls.load(), 1u);
            EXPECT_GT(stats.bytes_outstanding.load(), 0u);
        }
        EXPECT_EQ(stats.alloc_calls.load(), stats.dealloc_calls.load());
        EXPECT_EQ(stats.bytes_outstanding.load(), 0u);
    }

    TEST(FlexArrayException, CountingAllocatorWithThrowOnNthZeroOutstanding)
    {
        lcf::test::AllocStats stats;
        using A = lcf::test::CountingAllocator<std::byte>;
        using FlexT = lcf::FlexArray_Opus_4_7<ThrowOnNth, std::uint32_t, A>;
        const std::size_t baseline = ThrowOnNth::s_alive_count.load();
        ThrowOnNth::reset(0);

        {
            ThrowOnNth proto(0);
            ThrowOnNth::reset(/*throw_at=*/5);
            EXPECT_THROW({
                FlexT a(10, proto, A{&stats});
                (void)a;
            }, std::runtime_error);
        }
        EXPECT_EQ(stats.alloc_calls.load(), stats.dealloc_calls.load());
        EXPECT_EQ(stats.bytes_outstanding.load(), 0u);
        EXPECT_EQ(ThrowOnNth::s_alive_count.load(), baseline);
        ThrowOnNth::reset(0);
    }

} // namespace
