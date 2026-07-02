#pragma once

#include <cstddef>

namespace lcf::details {

// std::hardware_destructive_interference_size is a compile-time constant that GCC
// flags (-Winterference-size) because it drifts with -mtune/-mcpu and is unstable
// across ABI boundaries. As types using it here cross shared-library boundaries,
// we pin our own per-architecture constant so all translation units agree on the
// layout. Values match Boost.Atomic's cache-line detection.
#if defined(__s390__) || defined(__s390x__)
    inline constexpr std::size_t k_cache_line_size = 256;
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
    inline constexpr std::size_t k_cache_line_size = 128;
#elif (defined(__aarch64__) || defined(__arm64__)) && defined(__APPLE__)
    inline constexpr std::size_t k_cache_line_size = 128;   // Apple M/A series
#else
    inline constexpr std::size_t k_cache_line_size = 64;
#endif

} // namespace lcf::details
