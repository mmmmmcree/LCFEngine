#include "log.h"
#include "tasks/TaskScheduler.h"
#include "download_and_save.h"
#include "UserCommandContext.h"
#include <boost/url.hpp>

namespace asio = boost::asio;
using namespace std::chrono_literals;

using Taskflow = tf::Taskflow;

int main()
{
    lcf::Logger::init();
    lcf::TaskScheduler scheduler;
    lcf::UserCommandContext user_command_context {scheduler.getIOContext()};
    // download --url https://dummyimage.com/800x600/FF0000/FFFFFF.jpg?text=Async --output-path downloaded_image.jpg

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
    scheduler.registerAsyncTask(download_image_pipeline(url), std::move(taskflow_getter));
    scheduler.registerAsyncTask(user_command_context.loop());
    lcf_log_info("Scheduler started");
    scheduler.run();
    lcf_log_info("Done");
    return 0;
}
