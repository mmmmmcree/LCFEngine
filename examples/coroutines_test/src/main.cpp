#include "log.h"

#include "tasks/TaskScheduler.h"
#include "download_and_save.h"
#include "UserCommandContext.h"

using namespace lcf;

asio::awaitable<void> async_task()
{
    boost::urls::url url {"https://dummyimage.com/800x600/FF0000/FFFFFF.jpg?text=Async"};
    auto body = co_await download_image_pipeline(url);

    tf::Executor executor {1};
    tf::Taskflow taskflow;
    taskflow.emplace(
        [&body]() {
            lcf_log_info("begin taskflow save");
            save_to(body, std::filesystem::path{"async.jpg"});
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            lcf_log_info("end taskflow save");
        }
    );
    auto io_executor = co_await asio::this_coro::executor;
    co_await make_awaitable(io_executor, executor, std::move(taskflow));
}

int main()
{
    lcf::Logger::init();
    int heartbeat = 0;
    lcf::PeriodicTask periodic_task {
        std::chrono::milliseconds(1000),
        [&]() { lcf_log_info("heartbeat {}", heartbeat++); },
        [&]() { return heartbeat < 10; },
        []() { lcf_log_info("Periodic task finished"); }
    };
    lcf::TaskScheduler scheduler;
    lcf::UserCommandContext user_cmd_context {scheduler.getIOContext()};
    // download --url https://dummyimage.com/800x600/FF0000/FFFFFF.jpg?text=Async --output-path downloaded_image.jpg
    scheduler.registerPeriodicTask(std::move(periodic_task))
        .registerAwaitable(async_task())
        .registerAwaitable(user_cmd_context.loop())
        .run();
    return 0;
}
