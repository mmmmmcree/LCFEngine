#include "IOContext.h"

using namespace lcf;

void IOContext::run()
{
    Base::run();
}

void IOContext::stop()
{
    Base::stop();
}

void ThreadIOContext::run()
{
    m_work_guard_opt.emplace(asio::make_work_guard(*this));
    m_thread_opt = std::jthread([this](std::stop_token stop_token) {
        std::stop_callback stop_callback(stop_token, [&]() {
            m_work_guard_opt->reset();
        });
        Base::run();
    });
}

void ThreadIOContext::stop()
{
    Base::stop();
    if (m_thread_opt) {
        m_thread_opt->request_stop();
    }
}