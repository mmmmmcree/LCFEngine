// Vector-like contract test, parameterized over multiple container instantiations.

#include <array/FlexArray.h>
#include "array/FlexArray_Opus_4_7.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

    // All three containers now share snake_case API surface.
    template <typename Container, typename V>
    void adapt_push(Container & c, V && v) { c.push_back(std::forward<V>(v)); }

    template <typename Container>
    void adapt_pop(Container & c) { c.pop_back(); }

    template <typename Container>
    bool adapt_empty(const Container & c) { return c.empty(); }

    template <typename Container>
    std::size_t adapt_size(const Container & c) { return c.size(); }

    template <typename Container>
    std::size_t adapt_capacity(const Container & c) { return c.capacity(); }

    template <typename Container>
    void adapt_reserve(Container & c, std::size_t n) { c.reserve(n); }

    template <typename Container>
    void adapt_clear(Container & c) { c.clear(); }

    template <typename Container>
    class VectorContract : public ::testing::Test {};

    using ContractTypes = ::testing::Types<
        std::vector<int>,
        lcf::FlexArray<int>,
        lcf::FlexArray<int, std::uint64_t>,
        lcf::FlexArray<int, std::uint16_t>,
        lcf::FlexArray_Opus_4_7<int>,
        lcf::FlexArray_Opus_4_7<int, std::uint64_t>,
        lcf::FlexArray_Opus_4_7<int, std::uint16_t>
    >;
    TYPED_TEST_SUITE(VectorContract, ContractTypes);

    TYPED_TEST(VectorContract, FullLifecycle)
    {
        TypeParam c;
        EXPECT_TRUE(adapt_empty(c));
        EXPECT_EQ(adapt_size(c), 0u);

        for (int i = 0; i < 100; ++i) { adapt_push(c, i); }
        EXPECT_EQ(adapt_size(c), 100u);
        EXPECT_FALSE(adapt_empty(c));

        for (int i = 0; i < 100; ++i) {
            EXPECT_EQ(c[static_cast<std::size_t>(i)], i);
        }

        int sum = 0;
        for (auto v : c) { sum += v; }
        EXPECT_EQ(sum, 99 * 100 / 2);

        for (int i = 0; i < 50; ++i) { adapt_pop(c); }
        EXPECT_EQ(adapt_size(c), 50u);
        EXPECT_EQ(c[49], 49);

        adapt_reserve(c, 1000);
        EXPECT_GE(adapt_capacity(c), 1000u);
        EXPECT_EQ(adapt_size(c), 50u);

        TypeParam copy = c;
        EXPECT_EQ(adapt_size(copy), adapt_size(c));
        for (std::size_t i = 0; i < adapt_size(c); ++i) {
            EXPECT_EQ(copy[i], c[i]);
        }

        TypeParam moved = std::move(copy);
        EXPECT_EQ(adapt_size(moved), 50u);

        adapt_clear(c);
        EXPECT_TRUE(adapt_empty(c));
        EXPECT_EQ(adapt_size(c), 0u);
    }

} // namespace
