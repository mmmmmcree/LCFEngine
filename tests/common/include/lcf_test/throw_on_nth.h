#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>

namespace lcf::test {

    // Throwing element type with controlled fail-on-Nth-construct.
    //
    //  s_alive_count is a global truth: it is incremented by every successful
    //  constructor and decremented by every destructor. reset() does NOT touch it
    //  (otherwise live instances elsewhere would corrupt the counter on destruction).
    //  reset() only re-arms the throw-trigger and zeroes the construct counter.
    struct ThrowOnNth
    {
        static std::atomic<std::size_t> s_construct_count;
        static std::size_t s_throw_at;          // 0 = disabled; otherwise throw when count reaches this
        static std::atomic<std::size_t> s_alive_count;

        static void reset(std::size_t throw_at = 0) noexcept
        {
            s_construct_count.store(0);
            s_throw_at = throw_at;
        }

        int value;

        ThrowOnNth() : value(0) { maybe_throw(); ++s_alive_count; }
        explicit ThrowOnNth(int v) : value(v) { maybe_throw(); ++s_alive_count; }
        ThrowOnNth(const ThrowOnNth & other) : value(other.value) { maybe_throw(); ++s_alive_count; }
        ThrowOnNth(ThrowOnNth && other) noexcept : value(other.value) { ++s_construct_count; ++s_alive_count; }
        ThrowOnNth & operator=(const ThrowOnNth & other) { value = other.value; return *this; }
        ThrowOnNth & operator=(ThrowOnNth && other) noexcept { value = other.value; return *this; }
        ~ThrowOnNth() { --s_alive_count; }

    private:
        void maybe_throw()
        {
            const std::size_t n = ++s_construct_count;
            if (s_throw_at != 0 and n == s_throw_at) { throw std::runtime_error("ThrowOnNth deliberate"); }
        }
    };

} // namespace lcf::test
