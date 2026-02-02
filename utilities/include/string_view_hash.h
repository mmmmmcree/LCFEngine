#pragma once

#include <string_view>

namespace lcf::detail {
    template <std::integral Int, typename Char>
    constexpr Int fnv1a_impl(const Char * str, size_t len, Int hash) noexcept
    {
        constexpr Int c_prime = 0x100000001b3ULL;
        return len == 0 ? hash : fnv1a_impl(str + 1, len - 1, (hash ^ static_cast<Int>(*str)) * c_prime);
    }

    template <std::integral Int, typename Char>
    constexpr Int fnv1a(std::basic_string_view<Char> str_view) noexcept
    {
        constexpr Int c_offset = 0xcbf29ce484222325ULL;
        return detail::fnv1a_impl<Int, Char>(str_view.data(), str_view.size(), c_offset);
    }
}

namespace lcf {
    template <std::integral Int = uint64_t>
    constexpr Int hash(std::string_view str) noexcept
    {
        return detail::fnv1a<Int, typename std::string_view::value_type>(str);
    }

    template <std::integral Int = uint64_t>
    constexpr Int hash(std::wstring_view str) noexcept
    {
        return detail::fnv1a<Int, typename std::wstring_view::value_type>(str);
    }
}