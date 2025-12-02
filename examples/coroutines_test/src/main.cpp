#include "log.h"
#include "tasks/TaskScheduler.h"

#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

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
    ssl_ctx.set_default_verify_paths();

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

    lcf_log_info("Scheduler started");
    scheduler.run();
    lcf_log_info("Done");
    return 0;
}
