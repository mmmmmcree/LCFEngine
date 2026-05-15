// Vector-like contract test, parameterized over multiple container instantiations.
// Mirrors runVectorContractTest<Container>() from the original Test.cpp.

#include <array/FlexArray.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

    // Adapter helpers: unify std::vector / lcf::FlexArray surface.
    template <typename T, std::unsigned_integral S, typename A>
    std::size_t adapt_size(const lcf::FlexArray<T, S, A> & c) noexcept { return c.size(); }
    template <typename T, typename A>
    std::size_t adapt_size(const std::vector<T, A> & c) noexcept { return c.size(); }

    template <typename T, std::unsigned_integral S, typename A>
    bool adapt_empty(const lcf::FlexArray<T, S, A> & c) noexcept { return c.isEmpty(); }
    template <typename T, typename A>
    bool adapt_empty(const std::vector<T, A> & c) noexcept { return c.empty(); }

    template <typename T, std::unsigned_integral S, typename A, typename V>
    void adapt_push(lcf::FlexArray<T, S, A> & c, V && v) { c.pushBack(std::forward<V>(v)); }
    template <typename T, typename A, typename V>
    void adapt_push(std::vector<T, A> & c, V && v) { c.push_back(std::forward<V>(v)); }

    template <typename T, std::unsigned_integral S, typename A>
    void adapt_pop(lcf::FlexArray<T, S, A> & c) { c.popBack(); }
    template <typename T, typename A>
    void adapt_pop(std::vector<T, A> & c) { c.pop_back(); }

    template <typename T, std::unsigned_integral S, typename A>
    void adapt_clear(lcf::FlexArray<T, S, A> & c) noexcept { c.clear(); }
    template <typename T, typename A>
    void adapt_clear(std::vector<T, A> & c) noexcept { c.clear(); }

    template <typename T, std::unsigned_integral S, typename A>
    void adapt_reserve(lcf::FlexArray<T, S, A> & c, std::size_t n) { c.reserve(n); }
    template <typename T, typename A>
    void adapt_reserve(std::vector<T, A> & c, std::size_t n) { c.reserve(n); }

    template <typename T, std::unsigned_integral S, typename A>
    std::size_t adapt_capacity(const lcf::FlexArray<T, S, A> & c) noexcept { return c.capacity(); }
    template <typename T, typename A>
    std::size_t adapt_capacity(const std::vector<T, A> & c) noexcept { return c.capacity(); }

    template <typename Container>
    class VectorContract : public ::testing::Test {};

    using ContractTypes = ::testing::Types<
        std::vector<int>,
        lcf::FlexArray<int>,
        lcf::FlexArray<int, std::uint64_t>,
        lcf::FlexArray<int, std::uint16_t>
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
