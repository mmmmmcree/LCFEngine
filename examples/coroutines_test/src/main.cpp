#include "log.h"
#include "tasks/TaskScheduler.h"

#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <CLI/CLI.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

using namespace std::chrono_literals;

static asio::awaitable<beast::multi_buffer> download_image_pipeline(asio::io_context& io, http::request<http::empty_body> req)
{
    auto host_field = req[http::field::host];
    if (host_field.empty()) {
        lcf_log_error("request missing Host header");
        co_return beast::multi_buffer {};
    }
    std::string host = std::string(host_field);
    std::string target = std::string(req.target());

    ssl::context ssl_ctx(ssl::context::tls_client);
#ifdef _WIN32
    ssl_ctx.load_verify_file("cacert.pem");
#else
    ssl_ctx.set_default_verify_paths();
#endif // _WIN32


    // 解析、连接、握手、发送请求（协程串行）
    tcp::resolver resolver(io);
    auto results = co_await resolver.async_resolve(host, "443", asio::use_awaitable);

    beast::ssl_stream<beast::tcp_stream> stream(io, ssl_ctx);
    stream.set_verify_mode(ssl::verify_peer);
    (void)SSL_set_tlsext_host_name(stream.native_handle(), host.c_str());

    co_await beast::get_lowest_layer(stream).async_connect(results, asio::use_awaitable);
    co_await stream.async_handshake(ssl::stream_base::client, asio::use_awaitable);

    if (req.find(http::field::user_agent) == req.end()) {
        req.set(http::field::user_agent, std::string("beast-coroutine/" + std::to_string(BOOST_BEAST_VERSION)));
    }
    if (req.find(http::field::accept) == req.end()) {
        req.set(http::field::accept, "*/*");
    }

    co_await http::async_write(stream, req, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::dynamic_body> res;
    co_await http::async_read(stream, buffer, res, asio::use_awaitable);

    if (res.result() != http::status::ok) {
        lcf_log_warn("HTTP status: {}", static_cast<unsigned>(res.result()));
        co_return beast::multi_buffer {};
    }

    co_return res.body();
}


#ifdef _WIN32
#include <boost/asio/windows/stream_handle.hpp>
#include <boost/asio/windows/object_handle.hpp>

#include <iostream>
namespace win32 {
    bool manipulate_console_input(std::string & input_buffer, HANDLE std_in_handle)
    {
        INPUT_RECORD record;
        DWORD read;
        if (not ::ReadConsoleInputW(std_in_handle, &record, 1, &read)) { return false; }
        if (record.EventType != KEY_EVENT or not record.Event.KeyEvent.bKeyDown) { return false; }
        bool input_available = false;
        switch (record.Event.KeyEvent.wVirtualKeyCode) {
            case VK_RETURN: {
                std::cout << std::endl;
                input_available = true;
            } break;
            case VK_BACK: {
                if (not input_buffer.empty()) {
                    input_buffer.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            } break;
            default: {
                char ch = static_cast<char>(record.Event.KeyEvent.uChar.AsciiChar);
                input_buffer += ch;
                std::cout << ch << std::flush;
            } break;
        }
        return input_available;
    }

    bool read_line_from_pipe(std::string & input_buffer, HANDLE std_in_handle)
    {
        DWORD word_count = 0;
        if (not ::PeekNamedPipe(std_in_handle, nullptr, 0, nullptr, &word_count, nullptr) or word_count == 0) {
            return false;
        }
        input_buffer.resize(word_count);
        DWORD n = 0;
        if (not ::ReadFile(std_in_handle, input_buffer.data(), word_count, &n, nullptr)) { return false; }
        if (not input_buffer.empty() and input_buffer.back() == '\n') { input_buffer.pop_back(); }
        if (not input_buffer.empty() and input_buffer.back() == '\r') { input_buffer.pop_back(); }
        return true;
    }
}

#else
#include <boost/asio/posix/stream_descriptor.hpp>
#include <unistd.h>
#endif // _WIN32

asio::awaitable<void> command_loop(asio::io_context & io_context)
{
    CLI::App main_cmd;

    auto parse_command_str = [&main_cmd](const std::string & command_str)
    {
        try {
            main_cmd.parse(command_str);
        } catch (const CLI::ParseError & e) {
            lcf_log_error("{}", e.what());
        }
    };

    auto quit_cmd = main_cmd.add_subcommand("quit", "quit the program");
    bool quit_flag = false;
    quit_cmd->callback([&quit_flag]() {
        lcf_log_info("quit command received");
        quit_flag = true;
    });

    auto download_cmd = main_cmd.add_subcommand("download", "download an image");
    std::string url;
    download_cmd->add_option("--url", url, "the URL of the image to download")->required();
    download_cmd->callback([&url]() {
        lcf_log_info("download command received for URL {}", url);
    });

#ifdef _WIN32
    HANDLE std_in_handle = ::GetStdHandle(STD_INPUT_HANDLE);
    DWORD std_in_type = ::GetFileType(std_in_handle);
    if (std_in_type == FILE_TYPE_PIPE or std_in_type == FILE_TYPE_DISK) {
        asio::steady_timer timer {io_context, 500ms};
        std::array<char, 256> buffer;
        std::string command_str;
        while (true) {
            if (not win32::read_line_from_pipe(command_str, std_in_handle)) {
                co_await timer.async_wait(asio::use_awaitable);
                continue;
            }
            if (command_str.empty()) { continue; }
            
            parse_command_str(command_str);
            if (quit_flag) { break; }
        }
    } else {
        boost::asio::windows::object_handle input_handle{io_context, std_in_handle};
        std::string command_str;
        while (true) {
            co_await input_handle.async_wait(asio::use_awaitable);
            bool command_available = win32::manipulate_console_input(command_str, std_in_handle);
            if (not command_available) { continue; }
            if (command_str.empty()) { continue; }

            parse_command_str(command_str);
            command_str.clear();
            if (quit_flag) { break; }
        }
    }
#else
#endif // _WIN32
    co_return;
}

using Taskflow = tf::Taskflow;

int main()
{
    lcf::Logger::init();
    lcf::TaskScheduler scheduler;

    uint32_t heartbeat_count = 0;
    scheduler.registerPeriodicTask(
        500ms,
        [&heartbeat_count]() { lcf_log_info("heartbeat {}", heartbeat_count++); },
        [&heartbeat_count]() { return heartbeat_count < 20; }
    );

    http::request<http::empty_body> req;
    req.version(11);
    req.method(http::verb::get);
    req.target("/800x600/FF0000/FFFFFF.jpg&text=Async");
    req.set(http::field::host, "dummyimage.com");

    auto async_task_getter = [req = std::move(req), &io = scheduler.getIOContext()]()
    {
        return download_image_pipeline(io, req);
    };

    auto taskflow_getter = [](beast::multi_buffer body) -> Taskflow
    {
        Taskflow tf;
        auto [A, B] = tf.emplace(
            [body_sp = std::make_shared<beast::multi_buffer>(std::move(body))]() {
                lcf_log_info("Processing {} bytes", body_sp->size());
                std::vector<std::byte> bytes(body_sp->size());
                asio::buffer_copy(asio::buffer(bytes), body_sp->data());
                std::ofstream ofs("downloaded.jpg", std::ios::binary);
                ofs.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                std::this_thread::sleep_for(3s);
                lcf_log_info("Saved image");
            },
            []() {
                lcf_log_info("accompanying task step1 running..."); 
                std::this_thread::sleep_for(1.5s);
                lcf_log_info("accompanying task step1 done.");
                lcf_log_info("accompanying task step2 running..."); 
                std::this_thread::sleep_for(1.5s);
                lcf_log_info("accompanying task done.");
            }
        );
        return tf;
    };

    scheduler.registerAsyncTask(std::move(async_task_getter), std::move(taskflow_getter));

    asio::co_spawn(scheduler.getIOContext(), command_loop(scheduler.getIOContext()), asio::detached);
    lcf_log_info("Scheduler started");
    scheduler.run();
    lcf_log_info("Done");
    return 0;
}
