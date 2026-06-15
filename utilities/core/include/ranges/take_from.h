#pragma once

#include <ranges>

namespace lcf::details {
    template<std::ranges::random_access_range Indices>
    class take_from_helper
    {
    public:
        explicit take_from_helper(Indices indices) : m_indices(std::move(indices)) {}
        template<std::ranges::random_access_range Range>
        auto operator()(Range&& range) const
        {
            return m_indices | std::views::transform([r = std::forward<Range>(range)](auto idx) -> decltype(auto) { return r[idx]; });
        }
    private:
        Indices m_indices;
    };
}

namespace lcf {
    template<std::ranges::random_access_range Range, std::ranges::range Indices>
    requires std::integral<std::ranges::range_value_t<Indices>>
    auto operator|(Range&& range, const details::take_from_helper<Indices>& adaptor)
    {
        return adaptor(std::forward<Range>(range));
    }
}

namespace lcf::views {
    template<std::ranges::range Indices>
    requires std::integral<std::ranges::range_value_t<Indices>>
    auto take_from(Indices && indices)
    {
        return details::take_from_helper<std::decay_t<Indices>>(std::forward<Indices>(indices));
    }
}