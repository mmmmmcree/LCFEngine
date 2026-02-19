#include <boost/icl/interval_map.hpp>

namespace lcf::icl {
    template <
        typename IntervalKey,
        typename IntervalValue,
        template<typename> typename Compare  = std::less, // IntervalKey
        template<typename> typename Combine  = boost::icl::inplace_plus, // IntervalValue
        template<typename> typename Section  = boost::icl::inter_section, // IntervalValue
        typename Interval = boost::icl::interval_type_default<IntervalKey, Compare>::type,
        template<typename> typename Allocator = std::allocator,
        typename Traits = boost::icl::partial_absorber
    >
    using interval_map = typename boost::icl::interval_map<IntervalKey, IntervalValue, Traits, Compare, Combine, Section, Interval, Allocator>;

    template <typename T>
    class DefaultIntervalWrapper
    {
        using Self = DefaultIntervalWrapper<T>;
    public:
        DefaultIntervalWrapper() = default;
        DefaultIntervalWrapper(T && value) : m_value(std::forward<T>(value)) {}
        bool operator==(const Self & other) const { return m_value == other.m_value; }
        Self & operator+=(const Self & other) { return *this; }
        const T & getValue() const { return m_value.value(); }
    private:
        std::optional<T> m_value;
    };
}