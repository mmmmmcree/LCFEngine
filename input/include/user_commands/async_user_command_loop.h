#pragma once

#include "IUserCommandContext.h"
#include <boost/asio/awaitable.hpp>

namespace lcf {
    boost::asio::awaitable<void> async_user_command_loop(IUserCommandContext & user_command_context);
}