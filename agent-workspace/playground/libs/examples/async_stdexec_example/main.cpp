#include <async/task.h>
#include <iostream>
#include <thread>

int main()
{
    namespace task = lcf::async::task;
    lcf::async::Context control;
    lcf::async::Context worker;
    std::jthread worker_thread {[&] { worker.run(); }};
    auto image = task::Chain {20, worker.getExecutor()}.link([](const int & value) -> lcf::async::Awaitable<int> {
        co_await lcf::async::delay(std::chrono::milliseconds {1});
        co_return value + 1;
    });
    auto shader = task::Chain {21, worker.getExecutor()}.link([](const int & value) { return value; });
    auto joined = task::merge(image.getResultToken(), shader.getResultToken()).link(control.getExecutor(),
        [](const int & image, const int & shader) { return image + shader; });
    bool finished = false;
    int exit_code = 1;
    std::move(joined).start(control.getExecutor(), [&](const task::Result<int> & result) {
        finished = true;
        if (result) { std::cout << "image + shader = " << *result << '\n'; exit_code = 0; }
    });
    std::move(image).start(control.getExecutor(), [](const auto &) {});
    std::move(shader).start(control.getExecutor(), [](const auto &) {});
    while (not finished) {
        const auto report = control.poll({.m_max_callbacks = 32, .m_max_duration = std::chrono::microseconds {200}});
        if (report.m_callbacks == 0u) { std::this_thread::yield(); }
    }
    control.runUntilIdle();
    worker.stop();
    return exit_code;
}
