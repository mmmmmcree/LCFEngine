#include "user_command_loop.h"
#include "log.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/url.hpp>
#include <CLI/CLI.hpp>

using namespace lcf;
namespace asio = boost::asio;
namespace urls = boost::urls;

#ifdef _WIN32

#include <windows.h>
#include <boost/asio/windows/object_handle.hpp>
#include <iostream>

asio::awaitable<std::string> read_line_from_console(asio::windows::object_handle & console);

asio::awaitable<std::string> read_line_from_pipe(HANDLE stdin_handle);

#else

#include <unistd.h>
#include <boost/asio/posix/stream_descriptor.hpp>

asio::awaitable<std::string> read_line_from_posix(asio::posix::stream_descriptor & stdin_descriptor)

#endif // _WIN32

asio::awaitable<void> lcf::user_command_loop()
{
    bool loop_continue = true;

    CLI::App main_cmd;

    auto parse_command_str = [&main_cmd](const std::string & command_str)
    {
        std::string trimmed = boost::algorithm::trim_copy(command_str);
        try {
            main_cmd.parse(trimmed);
        } catch (const CLI::ParseError & e) {
            lcf_log_warn("{}", e.what());
        }
    };

    auto quit_cmd = main_cmd.add_subcommand("quit", "quit the program");
    bool quit_flag = false;
    quit_cmd->callback([&loop_continue]() {
        lcf_log_info("quit command received");
        loop_continue = false;
    });

    auto download_cmd = main_cmd.add_subcommand("download", "download an image");
    std::string url;
    download_cmd->add_option("--url", url, "the URL of the image to download")->required();
    download_cmd->callback([&url]() {
        lcf_log_info("download command received: {}", url);
        auto result = urls::parse_uri(url);
        if (not result) {
            lcf_log_warn("invalid URL: {}", url);
            return;
        }
        urls::url_view u_view = result.value();
        urls::url u(u_view);

        // asio::post(io_context, [&io_context, u = std::move(u)]() mutable {
        //     asio::co_spawn(io_context, [u = std::move(u), &io_context]() -> asio::awaitable<void> {
        //         beast::multi_buffer body = co_await download_image_pipeline(io_context, u);
        //         save_to(std::move(body), "a.jpg");
        //         lcf_log_info("image saved");
        //         co_return;
        //     }, asio::detached);
        // });
    });


#ifdef _WIN32
    HANDLE stdin_handle = ::GetStdHandle(STD_INPUT_HANDLE);
    if (stdin_handle == INVALID_HANDLE_VALUE) {
        std::runtime_error error {"GetStdHandle Failed"};
        lcf_log_error(error.what());
        throw error;
    }
    DWORD file_type = ::GetFileType(stdin_handle);
    if (file_type == FILE_TYPE_CHAR) {
        DWORD mode;
        ::GetConsoleMode(stdin_handle, &mode);
        ::SetConsoleMode(stdin_handle, mode | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        asio::windows::object_handle console {co_await asio::this_coro::executor, stdin_handle};
        while (loop_continue) {
            parse_command_str(co_await read_line_from_console(console));
        }
    } else if (file_type == FILE_TYPE_PIPE || file_type == FILE_TYPE_DISK) {
        while (loop_continue) {
            parse_command_str(co_await read_line_from_pipe(stdin_handle));
        }
    }
#else
    asio::posix::stream_descriptor sd{co_await asio::this_coro::executor, ::dup(STDIN_FILENO)};
    while (loop_continue) {
        parse_command_str(co_await read_line_from_posix(sd));
    }
#endif // _WIN32
    co_return;
}

#ifdef _WIN32
asio::awaitable<std::string> read_line_from_console(asio::windows::object_handle & console)
{
    HANDLE console_handle = console.native_handle();
    std::string line;
    bool read_line_finished = false;
    while (not read_line_finished) {
        co_await console.async_wait(asio::use_awaitable);
        INPUT_RECORD input_records[128];
        DWORD events_read = 0;
        if (not ::ReadConsoleInput(console_handle, input_records, std::size(input_records), &events_read)) {
            std::runtime_error error {"ReadConsoleInput Failed"};
            lcf_log_error(error.what());
            throw error;
        }
        for (DWORD i = 0; i < events_read; ++i) {
            if (input_records[i].EventType != KEY_EVENT or not input_records[i].Event.KeyEvent.bKeyDown) { continue; }
            char ch = input_records[i].Event.KeyEvent.uChar.AsciiChar;
            if (ch == '\r') {
                read_line_finished = true;
                std::cout << std::endl;
            } else if (std::isprint(ch)) {
                line += ch;
                std::cout << ch;
            } else if (ch == '\b' and not line.empty()) {
                line.pop_back();
                std::cout << "\b \b";
            }
        }
    }
    co_return line;
}

asio::awaitable<std::string> read_line_from_pipe(HANDLE stdin_handle)
{
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer {executor, std::chrono::milliseconds(500)};
    std::string line;
    bool read_line_finished = false;
    while (not read_line_finished) {
        DWORD available_bytes = 0;
        if (not ::PeekNamedPipe(stdin_handle, nullptr, 0, nullptr, &available_bytes, nullptr)) {
            throw std::runtime_error("PeekNamedPipe failed");
        }
        if (available_bytes == 0) {
            co_await timer.async_wait(asio::use_awaitable);
            continue;
        }
        std::vector<char> buffer(available_bytes);
        DWORD bytes_read = 0;
        if (not ::ReadFile(stdin_handle, buffer.data(), available_bytes, &bytes_read, nullptr)) {
            std::runtime_error error {"ReadFile failed"};
            lcf_log_error(error.what());
            throw error;
        }
        for (DWORD i = 0; i < bytes_read; ++i) {
            char ch = buffer[i];
            if (ch == '\n') { read_line_finished = true; }
            else if (ch != '\r') { line += ch; }
        }
    }
    co_return line;
}

#else
#endif // _WIN32