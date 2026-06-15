#pragma once

#include "IUserCommandContext.h"
#include <asio/awaitable.hpp>

namespace lcf {
    asio::awaitable<void> async_user_command_loop(IUserCommandContext & user_command_context);
}