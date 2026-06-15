#pragma once

#include <taskflow/taskflow.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>

namespace lcf {
    asio::awaitable<void> make_awaitable(
        asio::any_io_executor & io_executor,
        tf::Executor & executor,
        tf::Taskflow taskflow);
}