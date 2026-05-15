#include <lcf_test/throw_on_nth.h>

namespace lcf::test {

    std::atomic<std::size_t> ThrowOnNth::s_construct_count{0};
    std::size_t ThrowOnNth::s_throw_at = 0;
    std::atomic<std::size_t> ThrowOnNth::s_alive_count{0};

} // namespace lcf::test
