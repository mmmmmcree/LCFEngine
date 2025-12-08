#pragma once

#include <taskflow/taskflow.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>

namespace lcf {
    namespace asio = boost::asio;

    asio::awaitable<void> make_awaitable(
        asio::any_io_executor & io_executor,
        tf::Executor & executor,
        tf::Taskflow taskflow);
}