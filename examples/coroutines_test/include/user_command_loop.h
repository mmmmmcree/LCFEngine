#pragma once

#include <boost/asio/awaitable.hpp>

namespace lcf {
    boost::asio::awaitable<void> user_command_loop();
}