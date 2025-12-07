#include "UserCommandContext.h"
#include "user_commands/async_user_command_loop.h"
#include <boost/algorithm/string.hpp>
#include <boost/url.hpp>
#include <boost/asio.hpp>
#include "log.h"
#include "download_and_save.h"

namespace urls = boost::urls;
namespace asio = boost::asio;
namespace beast = boost::beast;

lcf::UserCommandContext::UserCommandContext(boost::asio::io_context &io_context)
{
    m_main_cmd.description("Async Task Scheduler Demo - Supports async user input, async image download, and periodic heartbeat tasks");
    m_main_cmd.footer(
        "Program Features:\n"
        "  1. Periodic Heartbeat: Logs heartbeat messages at specified intervals\n"
        "  2. Async Image Download: Downloads an image from the specified URL and saves it locally\n"
        "  3. Async User Commands: Supports real-time user command input for program control\n"
        "\n"
        "Note: After startup, you can interact with the program via command-line input (see UserCommandContext implementation for available commands)"
    );

    auto quit_cmd = m_main_cmd.add_subcommand("quit", "Quit the program");
    quit_cmd->callback([this]() {
        m_active = false;  
    });

    auto download_cmd = m_main_cmd.add_subcommand("download", "download an image");
    download_cmd->add_option("--url", m_download_url_str, "the URL of the image to download")->required();
    download_cmd->add_option("--output-path", m_save_path_str, "the path to save the downloaded image")->required();
    download_cmd->callback([this, &io_context]() {
        lcf_log_info("download command received: {}", m_download_url_str);
        auto result = urls::parse_uri(m_download_url_str);
        if (not result) {
            lcf_log_warn("invalid URL: {}", m_download_url_str);
            return;
        }
        asio::co_spawn(io_context, [u = urls::url {*result}, p = std::filesystem::path {m_save_path_str}]() -> asio::awaitable<void>
        {
            beast::multi_buffer body = co_await download_image_pipeline(u);
            save_to(std::move(body), p);
            lcf_log_info("image saved");
            co_return;
        }, asio::detached);
    });
}

void lcf::UserCommandContext::execute(const std::string & command_line) noexcept
{
    std::string trimmed_command_line = boost::trim_copy(command_line);
    try {
        m_main_cmd.parse(std::move(trimmed_command_line));
    } catch (const CLI::CallForHelp& e) {
        lcf_log_info("{}", m_main_cmd.help());
    } catch (const CLI::ParseError& e) {
        lcf_log_warn("{}", e.what());
    }
}

boost::asio::awaitable<void> lcf::UserCommandContext::loop()
{
    return async_user_command_loop(*this);
}
