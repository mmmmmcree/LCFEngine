#include "log.h"
#include "tasks/TaskScheduler.h"

#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <CLI/CLI.hpp>
#include "user_command_loop.h"
#include <boost/url.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
namespace urls = boost::urls;
using tcp = asio::ip::tcp;

using namespace std::chrono_literals;

static asio::awaitable<beast::multi_buffer> download_image_pipeline(asio::io_context& io, urls::url_view u)
{
    std::string host = std::string(u.host());
    if (host.empty()) {
        co_return beast::multi_buffer {};
    }
    std::string port = u.has_port() ? std::string(u.port()) : (u.scheme_id() == urls::scheme::https ? "443" : "80");
    std::string target = std::string(u.encoded_path());
    if (u.has_query()) { target += "?" + std::string(u.encoded_query()); }

    http::request<http::empty_body> req;
    req.version(11);
    req.method(http::verb::get);
    req.target(target);
    req.set(http::field::host, host);
    req.set(http::field::user_agent, std::string("beast-coroutine/" + std::to_string(BOOST_BEAST_VERSION)));
    req.set(http::field::accept, "*/*");

    tcp::resolver resolver(io);
    auto results = co_await resolver.async_resolve(host, port, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::dynamic_body> res;

    if (u.scheme_id() == urls::scheme::https) {
        ssl::context ssl_ctx(ssl::context::tls_client);
#ifdef _WIN32
        ssl_ctx.load_verify_file("cacert.pem");
#else
        ssl_ctx.set_default_verify_paths();
#endif
        beast::ssl_stream<beast::tcp_stream> stream(io, ssl_ctx);
        stream.set_verify_mode(ssl::verify_peer);
        (void)SSL_set_tlsext_host_name(stream.native_handle(), host.c_str());
        co_await beast::get_lowest_layer(stream).async_connect(results, asio::use_awaitable);
        co_await stream.async_handshake(ssl::stream_base::client, asio::use_awaitable);
        co_await http::async_write(stream, req, asio::use_awaitable);
        co_await http::async_read(stream, buffer, res, asio::use_awaitable);
    } else {
        beast::tcp_stream stream(io);
        co_await stream.async_connect(results, asio::use_awaitable);
        co_await http::async_write(stream, req, asio::use_awaitable);
        co_await http::async_read(stream, buffer, res, asio::use_awaitable);
    }

    if (res.result() != http::status::ok) {
        co_return beast::multi_buffer {};
    }
    co_return res.body();
}

#include <filesystem>
void save_to(const beast::multi_buffer & buffer, const std::filesystem::path& path)
{
    if (buffer.size() == 0) { return; }
    std::vector<std::byte> bytes {buffer.size()};
    asio::buffer_copy(asio::buffer(bytes), buffer.data());
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
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

    auto taskflow_getter = [](beast::multi_buffer body) -> Taskflow
    {
        Taskflow tf;
        tf.emplace(
            [body_sp = std::make_shared<beast::multi_buffer>(std::move(body))]() {
                lcf_log_info("Processing {} bytes", body_sp->size());
                save_to(*body_sp, "downloaded_image.jpg");
                std::this_thread::sleep_for(3s);
                lcf_log_info("image saved");
            }
        );
        return tf;
    };

    boost::urls::url url {"https://dummyimage.com/800x600/FF0000/FFFFFF.jpg?text=Async"};
    scheduler.registerAsyncTask(download_image_pipeline(scheduler.getIOContext(), url), std::move(taskflow_getter));
    scheduler.registerAsyncTask(lcf::user_command_loop());
    lcf_log_info("Scheduler started");
    scheduler.run();
    lcf_log_info("Done");
    return 0;
}
