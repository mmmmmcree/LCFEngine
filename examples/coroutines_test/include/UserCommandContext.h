#pragma once

#include "user_commands/IUserCommandContext.h"
#include <CLI/CLI.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

namespace lcf {
    class UserCommandContext : public IUserCommandContext
    {
    public:
        using UserCommand = CLI::App;
        UserCommandContext(boost::asio::io_context & io_context);
        UserCommandContext(const UserCommandContext &) = delete;
        UserCommandContext(UserCommandContext &&) = default;
        UserCommandContext & operator=(const UserCommandContext &) = delete;
        UserCommandContext & operator=(UserCommandContext &&) = default;
        ~UserCommandContext() override = default;
        void execute(const std::string & command_line) noexcept override;
        bool isActive() const noexcept override { return m_active; }
        boost::asio::awaitable<void> loop();
    private:
        UserCommand m_main_cmd;
        bool m_active;
        std::string m_download_url_str;
        std::string m_save_path_str;
    };
}