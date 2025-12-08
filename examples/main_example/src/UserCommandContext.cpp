#include "UserCommandContext.h"
#include "user_commands/async_user_command_loop.h"
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include "log.h"


lcf::UserCommandContext::UserCommandContext(boost::asio::io_context &io_context)
{
}

void lcf::UserCommandContext::execute(const std::string & command_line) noexcept
{
    std::string trimmed_command_line = boost::trim_copy(command_line);
    lcf_log_info("Received command: {}", trimmed_command_line);
    if (trimmed_command_line == "quit") { m_is_active = false; }
}

boost::asio::awaitable<void> lcf::UserCommandContext::loop()
{
    return async_user_command_loop(*this);
}
