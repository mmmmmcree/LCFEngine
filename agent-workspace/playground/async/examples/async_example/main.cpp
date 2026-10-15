#include <mutex>
#include <asio.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <expected>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <asio/thread_pool.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <syncstream>
#include <thread>

#include <async/task.h>

namespace {

namespace task = lcf::async::task;
using namespace std::chrono_literals;

asio::awaitable<std::string> prepare_asset(std::string name, std::chrono::milliseconds delay)
{
    std::osyncstream(std::cout) << "prepare " << name << " on worker " << std::this_thread::get_id() << '\n';
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(delay);
    co_await timer.async_wait(asio::use_awaitable);
    co_return name + " ready";
}

}

int main()
{
    try {
        asio::io_context render;
        asio::thread_pool workers {2};
        auto image = task::Chain {std::string {"image"}, workers.get_executor()}
            .link([](const std::string & name) { return prepare_asset(name, 15ms); });
        auto shader = task::Chain {std::string {"shader"}, workers.get_executor()}
            .link([](const std::string & name) { return prepare_asset(name, 10ms); });
        auto tasks = task::merge(std::move(image), std::move(shader))
            .link(render.get_executor(), [](const std::string & image, const std::string & shader) {
                std::cout << "merge on render " << std::this_thread::get_id() << '\n';
                return image + " + " + shader;
            });
        int exit_code = 1;
        std::move(tasks).start(render.get_executor(), [&](const task::Result<std::string> & result) {
            if (not result) {
                std::cerr << "async failed: " << result.error().message() << '\n';
                return;
            }
            std::cout << result.value() << '\n';
            exit_code = 0;
        });
        render.run();
        workers.join();
        return exit_code;
    } catch (const std::exception & error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
