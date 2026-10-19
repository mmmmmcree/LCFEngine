#pragma once

#include <async/core/Awaitable.h>
#include <async/core/operations.h>
#include <concepts>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lcf::async::task {

LCF_ASYNC_EXPORT
template <typename T>
using Result = std::expected<T, std::error_code>;

LCF_ASYNC_EXPORT
template <typename T>
using ResultHandle = std::shared_ptr<const Result<T>>;

namespace details {

template <typename... Types>
inline constexpr bool are_nothrow_move_constructible_v = (std::is_nothrow_move_constructible_v<Types> and ...);

inline std::error_code exception_error(std::exception_ptr exception) noexcept
{
    try { std::rethrow_exception(exception); }
    catch (const std::system_error & error) { return error.code(); }
    catch (...) { return std::make_error_code(std::errc::io_error); }
}

template <typename T>
struct result_to_arguments_traits
{
    using type = std::tuple<const T &>;
    static constexpr type get(const Result<T> & result) { return std::tie(result.value()); }
};

template <>
struct result_to_arguments_traits<void>
{
    using type = std::tuple<>;
    static constexpr type get(const Result<void> &) noexcept { return {}; }
};

template <typename T>
using result_to_arguments_t = typename result_to_arguments_traits<T>::type;

template <typename... Values>
using results_to_arguments_t = decltype(std::tuple_cat(std::declval<result_to_arguments_t<Values>>()...));

template <typename T>
constexpr auto result_to_arguments(const Result<T> & result) noexcept(noexcept(result_to_arguments_traits<T>::get(result)))
{
    return result_to_arguments_traits<T>::get(result);
}

template <typename... Values>
constexpr auto merge_results_to_arguments(const Result<Values> &... results)
    noexcept(noexcept(std::tuple_cat(result_to_arguments(results)...)))
{
    return std::tuple_cat(result_to_arguments(results)...);
}

template <typename T>
struct normalized_value_traits : std::type_identity<T> {};

template <typename T>
struct normalized_value_traits<Result<T>> : std::type_identity<T> {};

template <typename T, typename Context>
struct normalized_value_traits<exec::basic_task<T, Context>> : normalized_value_traits<T> {};

template <typename T>
struct normalized_value_traits<Awaitable<T>> : normalized_value_traits<T> {};

template <>
struct normalized_value_traits<std::error_code> : std::type_identity<void> {};

template <typename Return>
using normalized_value_t = typename normalized_value_traits<std::remove_cvref_t<Return>>::type;

template <typename Return>
using normalized_result_t = Result<normalized_value_t<Return>>;

template <typename T>
struct is_result_traits : std::false_type {};

template <typename T>
struct is_result_traits<Result<T>> : std::true_type {};

template <typename T>
using is_result_t = is_result_traits<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_result_v = is_result_t<T>::value;

template <typename T>
struct is_awaitable_traits : std::false_type {};

template <typename T, typename Context>
struct is_awaitable_traits<exec::basic_task<T, Context>> : std::true_type {};

template <typename T>
struct is_awaitable_traits<Awaitable<T>> : std::true_type {};

template <typename T>
using is_awaitable_t = is_awaitable_traits<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_awaitable_v = is_awaitable_t<T>::value;

template <typename T>
struct awaitable_value_traits;

template <typename T, typename Context>
struct awaitable_value_traits<exec::basic_task<T, Context>> : std::type_identity<T> {};

template <typename T>
struct awaitable_value_traits<Awaitable<T>> : std::type_identity<T> {};

template <typename T>
using awaitable_value_t = typename awaitable_value_traits<std::remove_cvref_t<T>>::type;

template <typename T>
struct is_unexpected_traits : std::false_type {};

template <typename T>
struct is_unexpected_traits<std::unexpected<T>> : std::true_type {};

template <typename T>
using is_unexpected_t = is_unexpected_traits<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_unexpected_v = is_unexpected_t<T>::value;

template <typename T>
inline constexpr bool is_error_code_v = std::same_as<std::remove_cvref_t<T>, std::error_code>;

template <typename Return>
inline constexpr bool is_nothrow_result_conversion_v = std::is_void_v<Return> or is_error_code_v<Return>
    or std::is_nothrow_constructible_v<normalized_result_t<Return>, std::add_rvalue_reference_t<Return>>;

template <typename T>
concept result_value_c = std::same_as<T, void> or
    (std::is_object_v<T> and not std::is_array_v<T> and std::move_constructible<T>
    and not is_unexpected_v<T>
    and not std::same_as<std::remove_cv_t<T>, std::in_place_t>
    and not std::same_as<std::remove_cv_t<T>, std::unexpect_t>);

}

LCF_ASYNC_EXPORT
template <details::result_value_c T, typename Owner>
ResultHandle<T> make_result_handle(std::shared_ptr<Owner> owner_sp, const Result<T> * result_p) noexcept
{
    return ResultHandle<T> {std::move(owner_sp), result_p};
}

LCF_ASYNC_EXPORT
template <details::result_value_c T> class Chain;
LCF_ASYNC_EXPORT
template <details::result_value_c... Values> class ChainJunction;

namespace details {

template <typename Return>
struct supported_return_traits : std::bool_constant<
    result_value_c<normalized_value_t<Return>> and
    (std::is_void_v<Return> or std::constructible_from<std::remove_cvref_t<Return>, Return>)> {};

template <typename T, typename Context>
struct supported_return_traits<exec::basic_task<T, Context>> : std::bool_constant<
    std::same_as<Context, exec::default_task_context<T>> and supported_return_traits<T>::value
        and not is_awaitable_v<T>> {};

template <typename T>
struct supported_return_traits<Awaitable<T>> : std::bool_constant<supported_return_traits<T>::value
    and not is_awaitable_v<T>> {};

template <typename Return>
using is_supported_return_t = supported_return_traits<Return>;

template <typename Return>
inline constexpr bool is_supported_return_v = is_supported_return_t<Return>::value;

template <typename Function, typename Arguments>
struct invocation_traits {};

template <typename Function, typename... Arguments>
requires std::invocable<Function &, Arguments...>
struct invocation_traits<Function, std::tuple<Arguments...>>
{
    using return_type = std::invoke_result_t<Function &, Arguments...>;
    static constexpr bool is_nothrow_result = [] {
        if constexpr (is_supported_return_v<return_type>) {
            return std::is_nothrow_invocable_v<Function &, Arguments...> and is_nothrow_result_conversion_v<return_type>;
        } else { return false; }
    }();
};

template <typename Function, typename Arguments>
using invocation_return_t = typename invocation_traits<Function, Arguments>::return_type;

template <typename Function, typename Arguments>
using normalized_invocation_value_t = normalized_value_t<invocation_return_t<Function, Arguments>>;

template <typename Function, typename Arguments>
using normalized_invocation_return_t = normalized_result_t<invocation_return_t<Function, Arguments>>;

template <typename Function, typename Arguments>
concept function_c = std::move_constructible<Function>
    and requires { typename invocation_return_t<Function, Arguments>; }
    and is_supported_return_v<invocation_return_t<Function, Arguments>>;

template <typename Function, typename... Inputs>
concept task_function_c = function_c<Function, results_to_arguments_t<Inputs...>>;

template <typename Function, typename... Inputs>
using function_return_t = invocation_return_t<Function, results_to_arguments_t<Inputs...>>;

template <typename Function, typename... Inputs>
using function_value_t = normalized_invocation_value_t<Function, results_to_arguments_t<Inputs...>>;

template <typename Completion, typename Value>
concept completion_c = std::move_constructible<Completion> and std::invocable<Completion &, const Result<Value> &>;

template <typename Return>
constexpr normalized_result_t<Return> as_task_result(Return && result) noexcept(is_nothrow_result_conversion_v<Return>)
{
    if constexpr (is_error_code_v<Return>) {
        if (result) { return std::unexpected(result); }
        return {};
    } else if constexpr (is_result_v<Return>) {
        return std::forward<Return>(result);
    } else {
        return normalized_result_t<Return> {std::in_place, std::forward<Return>(result)};
    }
}

template <typename T>
auto stopped_as_error(T operation)
{
    return stde::let_stopped(std::move(operation), [] {
        return stde::just_error(std::make_exception_ptr(std::system_error(std::make_error_code(std::errc::operation_canceled))));
    });
}

template <typename Function, typename Arguments>
requires (function_c<Function, Arguments> and not is_awaitable_v<invocation_return_t<Function, Arguments>>)
constexpr normalized_invocation_return_t<Function, Arguments>
invoke_task_result(Function & function, Arguments & arguments)
    noexcept(invocation_traits<Function, Arguments>::is_nothrow_result)
{
    using Return = invocation_return_t<Function, Arguments>;
    if constexpr (std::is_void_v<Return>) {
        std::apply(function, arguments);
        return {};
    } else {
        return as_task_result(std::apply(function, arguments));
    }
}

// The awaiting node or retry coroutine keeps both inputs alive until completion.
template <result_value_c T>
class InvocationSender
{
    using ValueResult = Result<T>;
    struct InvocationRef
    {
        void * m_function_p;
        void * m_arguments_p;
        ValueResult (*m_invoke)(void *, void *);
        ValueResult operator()() const { return m_invoke(m_function_p, m_arguments_p); }
    };
    template <typename Receiver>
    class Operation
    {
        using Self = Operation;
        struct ScheduledReceiver
        {
            using receiver_concept = stde::receiver_t;
            stde::env_of_t<Receiver> get_env() const noexcept { return stde::get_env(m_operation_p->m_receiver); }
            void set_value() noexcept { m_operation_p->invoke(); }
            void set_error(std::exception_ptr exception) noexcept { stde::set_error(std::move(m_operation_p->m_receiver), std::move(exception)); }
            void set_stopped() noexcept { stde::set_stopped(std::move(m_operation_p->m_receiver)); }
            Self * m_operation_p;
        };
        using ScheduleSender = decltype(stde::schedule(stde::get_start_scheduler(stde::get_env(std::declval<const Receiver &>()))));
        using ScheduledOperation = stde::connect_result_t<ScheduleSender, ScheduledReceiver>;
    public:
        using operation_state_concept = stde::operation_state_t;
    public:
        ~Operation() noexcept = default;
        Operation(InvocationRef invocation, Receiver receiver) :
            m_invocation(std::move(invocation)), m_receiver(std::move(receiver)),
            m_operation(stde::connect(stde::schedule(stde::get_start_scheduler(stde::get_env(m_receiver))), ScheduledReceiver {this})) {}
        Operation(const Self &) = delete;
        Operation(Self &&) = delete;
        Self & operator=(const Self &) = delete;
        Self & operator=(Self &&) = delete;
    public:
        void start() noexcept { stde::start(m_operation); }
    private:
        void invoke() noexcept
        {
            try {
                if (stde::get_stop_token(stde::get_env(m_receiver)).stop_requested()) {
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
                }
                auto result = m_invocation();
                stde::set_value(std::move(m_receiver), std::move(result));
            } catch (...) { stde::set_error(std::move(m_receiver), std::current_exception()); }
        }
    private:
        InvocationRef m_invocation;
        Receiver m_receiver;
        ScheduledOperation m_operation;
    };
public:
    using sender_concept = stde::sender_t;
    using completion_signatures = stde::completion_signatures<stde::set_value_t(ValueResult), stde::set_error_t(std::exception_ptr), stde::set_stopped_t()>;
public:
    template <typename Function, typename Arguments>
    InvocationSender(Function & function, Arguments & arguments) noexcept :
        m_invocation {std::addressof(function), std::addressof(arguments),
            [](void * function_p, void * arguments_p) {
                return invoke_task_result(*static_cast<Function *>(function_p), *static_cast<Arguments *>(arguments_p));
            }} {}
public:
    template <stde::receiver Receiver>
    auto connect(Receiver receiver) && { return Operation<Receiver> {std::move(m_invocation), std::move(receiver)}; }
private:
    InvocationRef m_invocation;
};

// invoke_task owns the initial scheduler hop. This sender only binds its target;
// its completion affinity stays unknown because that target may differ from the caller.
template <result_value_c T>
class InvocationTaskSender
{
    using Task = Awaitable<Result<T>>;
public:
    using sender_concept = stde::sender_t;
    using completion_signatures = stde::completion_signatures<stde::set_value_t(Result<T>), stde::set_error_t(std::exception_ptr), stde::set_stopped_t()>;
public:
    InvocationTaskSender(Executor executor, Task task) noexcept : m_executor(std::move(executor)), m_task(std::move(task)) {}
public:
    template <stde::receiver Receiver>
    auto connect(Receiver receiver) &&
    {
        return stde::connect(stde::write_env(std::move(m_task), stde::prop {stde::get_start_scheduler, std::move(m_executor)}), std::move(receiver));
    }
private:
    Executor m_executor;
    Task m_task;
};

template <result_value_c T>
auto spawn_result(Executor executor, Awaitable<Result<T>> operation)
{
    return stopped_as_error(InvocationTaskSender<T> {std::move(executor), std::move(operation)});
}

template <result_value_c T>
auto spawn_result(Executor executor, InvocationSender<T> operation)
{
    return stopped_as_error(stde::write_env(std::move(operation), stde::prop {stde::get_start_scheduler, std::move(executor)}));
}

template <typename Function, typename Arguments>
requires (function_c<Function, Arguments> and not is_awaitable_v<invocation_return_t<Function, Arguments>>)
auto invoke_task(Function & function, Arguments & arguments)
{
    return InvocationSender<normalized_invocation_value_t<Function, Arguments>> {function, arguments};
}

template <typename Function, typename Arguments>
requires (function_c<Function, Arguments> and is_awaitable_v<invocation_return_t<Function, Arguments>>)
Awaitable<normalized_invocation_return_t<Function, Arguments>>
invoke_task(Function & function, Arguments & arguments)
{
    using Return = invocation_return_t<Function, Arguments>;
    co_await ::lcf::async::execution::details::YieldAwaitable {};
    const auto stop_token = co_await stde::read_env(stde::get_stop_token);
    if (stop_token.stop_requested()) { throw std::system_error(std::make_error_code(std::errc::operation_canceled)); }
    if constexpr (std::is_void_v<awaitable_value_t<Return>>) {
        co_await stopped_as_error(std::apply(function, arguments));
        co_return Result<void> {};
    } else {
        co_return as_task_result(co_await stopped_as_error(std::apply(function, arguments)));
    }
}

}

}
