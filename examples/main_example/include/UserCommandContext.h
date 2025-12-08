#pragma once

#include "user_commands/IUserCommandContext.h"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <functional>

namespace lcf {
    class UserCommandContext : public IUserCommandContext
    {
    public:
        using ActivePredicate = std::function<bool()>;
        UserCommandContext(boost::asio::io_context & io_context);
        UserCommandContext(const UserCommandContext &) = delete;
        UserCommandContext(UserCommandContext &&) = default;
        UserCommandContext & operator=(const UserCommandContext &) = delete;
        UserCommandContext & operator=(UserCommandContext &&) = default;
        ~UserCommandContext() override = default;
        void execute(const std::string & command_line) noexcept override;
        bool isActive() const noexcept override { return m_is_active; }
        boost::asio::awaitable<void> loop();
    private:
        bool m_is_active = true;
    };
}