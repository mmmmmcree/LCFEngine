#include "../support/Runtime.h"
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/steady_timer.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using namespace std::chrono_literals;

TEST(ErrorHandling, RetriesOnlyTheCurrentNodeWithStableInputs)
{
    Runtime runtime;
    const auto control_thread = std::this_thread::get_id();
    const auto failure = std::make_error_code(std::errc::resource_unavailable_try_again);
    unsigned prefix_runs = 0;
    unsigned attempts = 0;
    unsigned handled = 0;
    unsigned successor_runs = 0;
    const int * input_p = nullptr;
    auto seed = task::Chain {std::make_unique<int>(7), runtime.cpu()};
    auto original_rh = seed.getResult();
    auto prefix = std::move(seed).link([&](const std::unique_ptr<int> & value_up) {
        ++prefix_runs;
        return *value_up;
    });
    auto current = std::move(prefix).link(runtime.cpu(),
        [state_up = std::make_unique<unsigned>(0), &attempts, &input_p, control_thread, failure]
        (const int & value, const std::unique_ptr<int> & original_up) -> task::Result<int> {
            EXPECT_NE(std::this_thread::get_id(), control_thread);
            ++attempts;
            EXPECT_EQ(++*state_up, attempts);
            if (input_p) { EXPECT_EQ(input_p, &value); }
            input_p = &value;
            EXPECT_EQ(value, *original_up);
            if (attempts < 3) { return std::unexpected(failure); }
            return value + *original_up;
        }, task::on_error([state_up = std::make_unique<unsigned>(0), &handled, control_thread, failure]
            (const task::ErrorContext & context) {
            EXPECT_NE(std::this_thread::get_id(), control_thread);
            EXPECT_EQ(context.getErrorCode(), failure);
            EXPECT_EQ(context.getAttemptCount(), ++*state_up);
            ++handled;
            return task::retry_after(1ms);
        }), original_rh);
    auto recovered_rh = current.getResult();
    auto tail = std::move(current).link(runtime.control(), [&](const int & value, const int & repeated) {
        EXPECT_EQ(&value, &repeated);
        ++successor_runs;
        return value + 1;
    }, recovered_rh);
    const auto started = std::chrono::steady_clock::now();
    auto result_rv = runtime.run(std::move(tail));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 15);
    EXPECT_EQ(prefix_runs, 1u);
    EXPECT_EQ(attempts, 3u);
    EXPECT_EQ(handled, 2u);
    EXPECT_EQ(successor_runs, 1u);
    EXPECT_GE(std::chrono::steady_clock::now() - started, 2ms);
}

TEST(ErrorHandling, ExhaustedRetryPolicyPropagatesFailure)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::resource_unavailable_try_again);
    unsigned attempts = 0;
    unsigned handled = 0;
    unsigned successor_runs = 0;
    auto chain = task::Chain {runtime.cpu()}
        .link([&]() -> task::Result<int> { ++attempts; return std::unexpected(failure); },
            task::on_error([&](const task::ErrorContext & context) -> task::ErrorAction<int> {
                ++handled;
                if (context.getAttemptCount() < 3) { return task::retry_after(0ms); }
                return task::propagate(context.getErrorCode());
            }))
        .link([&](const int &) { ++successor_runs; });
    auto result_rv = runtime.run(std::move(chain));
    ASSERT_FALSE(*result_rv);
    EXPECT_EQ(result_rv->error(), failure);
    EXPECT_EQ(attempts, 3u);
    EXPECT_EQ(handled, 3u);
    EXPECT_EQ(successor_runs, 0u);
}

TEST(ErrorHandling, CancellationEndsTheRetryDelayWithoutCallingTheHandlerAgain)
{
    asio::io_context context;
    asio::cancellation_signal cancellation;
    unsigned attempts = 0;
    unsigned handled = 0;
    bool completed = false;
    std::exception_ptr exception;
    auto chain = task::Chain {context.get_executor()}.link([&]() -> task::Result<int> {
        ++attempts;
        return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }, task::on_error([&](const task::ErrorContext &) {
        ++handled;
        asio::post(context, [&] { cancellation.emit(asio::cancellation_type::terminal); });
        return task::retry_after(1h);
    }));
    asio::co_spawn(context, std::move(chain).run(), asio::bind_cancellation_slot(cancellation.slot(),
        [&](std::exception_ptr error, task::ResultView<int>) { exception = error; completed = true; }));
    context.run_for(1s);
    ASSERT_TRUE(completed);
    ASSERT_TRUE(exception);
    try {
        std::rethrow_exception(exception);
    } catch (const std::system_error & error) {
        EXPECT_EQ(error.code(), asio::error::operation_aborted);
    }
    EXPECT_EQ(attempts, 1u);
    EXPECT_EQ(handled, 1u);
}

TEST(ErrorHandling, MoveOnlyFallbackBecomesTheNodesPublishedResult)
{
    Runtime runtime;
    auto current = task::Chain {runtime.cpu()}.link([]() -> task::Result<std::unique_ptr<int>> {
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }, task::on_error([fallback_up = std::make_unique<int>(42)](const task::ErrorContext &) mutable {
        return task::use_value(std::move(fallback_up));
    }));
    auto recovered_rh = current.getResult();
    auto tail = std::move(current).link(
        [](const std::unique_ptr<int> & value_up, const std::unique_ptr<int> & repeated_up) {
            EXPECT_EQ(&value_up, &repeated_up);
            return *value_up;
        }, recovered_rh);
    auto result_rv = runtime.run(std::move(tail));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 42);
}

TEST(ErrorHandling, RecoveryPreservesVoidAndErrorCodePayloads)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    auto empty = task::Chain {runtime.cpu()}
        .link([&] { return failure; }, task::on_error([](const task::ErrorContext &) { return task::use_value(); }))
        .link([] { return 8; });
    EXPECT_EQ(runtime.run(std::move(empty))->value(), 8);
    auto code = task::Chain {runtime.cpu()}.link([&]() -> task::Result<std::error_code> {
        return std::unexpected(failure);
    }, task::on_error([&](const task::ErrorContext &) { return task::use_value(failure); }));
    auto result_rv = runtime.run(std::move(code));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), failure);
}

TEST(ErrorHandling, UpstreamFailureSkipsTheTaskAndItsHandler)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    unsigned task_runs = 0;
    unsigned handled = 0;
    auto chain = task::Chain {runtime.cpu()}
        .link([&]() -> task::Result<int> { return std::unexpected(failure); })
        .link([&](const int &) { ++task_runs; return 0; },
            task::on_error([&](const task::ErrorContext &) { ++handled; return task::use_value(42); }));
    auto result_rv = runtime.run(std::move(chain));
    ASSERT_FALSE(*result_rv);
    EXPECT_EQ(result_rv->error(), failure);
    EXPECT_EQ(task_runs, 0u);
    EXPECT_EQ(handled, 0u);
}

TEST(ErrorHandling, PropagationPreservesZeroErrorCodeAndAllowsReplacement)
{
    Runtime runtime;
    unsigned handled = 0;
    unsigned successor_runs = 0;
    auto zero = task::Chain {runtime.cpu()}
        .link([]() -> task::Result<int> { return std::unexpected(std::error_code {}); },
            task::on_error([&](const task::ErrorContext & context) {
                ++handled;
                EXPECT_EQ(context.getAttemptCount(), 1u);
                EXPECT_FALSE(context.getErrorCode());
                return task::propagate(context.getErrorCode());
            }))
        .link([&](const int &) { ++successor_runs; });
    auto zero_rv = runtime.run(std::move(zero));
    ASSERT_FALSE(*zero_rv);
    EXPECT_FALSE(zero_rv->error());
    EXPECT_EQ(handled, 1u);
    EXPECT_EQ(successor_runs, 0u);
    const auto replacement = std::make_error_code(std::errc::io_error);
    auto replaced = task::Chain {runtime.cpu()}.link([]() -> task::Result<int> {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }, task::on_error([&](const task::ErrorContext &) { return task::propagate(replacement); }));
    auto replaced_rv = runtime.run(std::move(replaced));
    ASSERT_FALSE(*replaced_rv);
    EXPECT_EQ(replaced_rv->error(), replacement);
}

TEST(ErrorHandling, AsyncTaskAndHandlerUseTheNodesExecutor)
{
    Runtime runtime;
    const auto control_thread = std::this_thread::get_id();
    unsigned attempts = 0;
    unsigned handled = 0;
    auto chain = task::Chain {3, runtime.cpu()}.link(runtime.control(),
        [&](const int &) -> asio::awaitable<task::Result<int>> {
            EXPECT_EQ(std::this_thread::get_id(), control_thread);
            co_await asio::post(asio::use_awaitable);
            ++attempts;
            co_return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
        }, task::on_error([&](const task::ErrorContext & context) -> asio::awaitable<task::ErrorAction<int>> {
            EXPECT_EQ(std::this_thread::get_id(), control_thread);
            ++handled;
            asio::steady_timer timer {co_await asio::this_coro::executor};
            timer.expires_after(1ms);
            co_await timer.async_wait(asio::use_awaitable);
            if (context.getAttemptCount() < 2) { co_return task::retry_after(0ms); }
            co_return task::use_value(9);
        }));
    auto result_rv = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 9);
    EXPECT_EQ(attempts, 2u);
    EXPECT_EQ(handled, 2u);
}

TEST(ErrorHandling, TaskAndHandlerExceptionsPropagateWithoutRecursiveHandling)
{
    Runtime runtime;
    unsigned handled = 0;
    auto task_throws = task::Chain {runtime.cpu()}.link([]() -> task::Result<int> {
        throw std::runtime_error("task");
    }, task::on_error([&](const task::ErrorContext &) { ++handled; return task::use_value(0); }));
    EXPECT_THROW(runtime.run(std::move(task_throws)), std::runtime_error);
    EXPECT_EQ(handled, 0u);
    auto handler_throws = task::Chain {runtime.cpu()}.link([]() -> task::Result<int> {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }, task::on_error([&](const task::ErrorContext &) -> task::ErrorAction<int> {
        ++handled;
        throw std::logic_error("handler");
    }));
    EXPECT_THROW(runtime.run(std::move(handler_throws)), std::logic_error);
    EXPECT_EQ(handled, 1u);
}

TEST(ErrorHandling, SharedFlowPublishesOnlyTheFinalAttemptAndLinksCanRecover)
{
    Runtime runtime;
    unsigned attempts = 0;
    unsigned handled = 0;
    unsigned branch_handled = 0;
    auto source = task::as_flow(task::Chain {runtime.cpu()}.link([&]() -> task::Result<int> {
        if (++attempts < 3) { return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again)); }
        return 5;
    }, task::on_error([&](const task::ErrorContext &) { ++handled; return task::retry_after(0ms); })));
    auto left = source.link([](const int & value) { return value + 1; });
    auto right = source.link([](const int &) -> task::Result<int> {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }, task::on_error([&](const task::ErrorContext &) { ++branch_handled; return task::use_value(7); }));
    auto joined = task::merge(std::move(left), std::move(right))
        .link([](const int & a, const int & b) { return a + b; });
    auto result_rv = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 13);
    EXPECT_EQ(runtime.run(source)->value(), 5);
    EXPECT_EQ(attempts, 3u);
    EXPECT_EQ(handled, 2u);
    EXPECT_EQ(branch_handled, 1u);
}

}
