#pragma once

#include <boost/asio.hpp>
#include <optional>
#include <thread>

namespace lcf {
    namespace asio = boost::asio;

    class IOContext : public asio::io_context
    {
        using Base = asio::io_context;
    public:
        using Base::Base;
        virtual ~IOContext() = default;
        virtual void run();
        virtual void stop();
    };

    class ThreadIOContext : public IOContext
    {
        using Base = IOContext;
        using ExecutorWorkGuard = boost::asio::executor_work_guard<typename Base::executor_type>;
    public:
        using Base::Base;
        void run() override;
        void stop() override;
    private:
        std::optional<ExecutorWorkGuard> m_work_guard_opt;
        std::optional<std::jthread> m_thread_opt;
    };
}