#include "tasks/Taskflow.h"
#include "Taskflow.h"
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>

using namespace lcf;

asio::awaitable<void> lcf::make_awaitable(
    asio::any_io_executor &io_executor,
    tf::Executor &executor,
    tf::Taskflow taskflow)
{
    auto taskflow_sp = std::make_shared<tf::Taskflow>(std::move(taskflow));
    auto async_task = [&executor, taskflow_sp, &io_executor](auto handler) {
        auto handler_sp = std::make_shared<std::decay_t<decltype(handler)>>(std::move(handler));
        auto notify = taskflow_sp->emplace([handler_sp, &io_executor]() {
            asio::post(io_executor, [handler_sp]() { (*handler_sp)(); });
        });
        taskflow_sp->for_each_task([&](tf::Task t) {
            if (t.num_successors() > 0 or t == notify) { return; }
            t.precede(notify);
        });
        executor.run(*taskflow_sp);
    };
    return asio::async_initiate<void()>(std::move(async_task), asio::use_awaitable);
}