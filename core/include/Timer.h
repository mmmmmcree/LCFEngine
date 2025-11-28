#pragma once

#include <boost/asio.hpp>
#include "PointerDefs.h"
#include "log.h"

namespace lcf {
    class IOContext : public boost::asio::io_context, public STDPointerDefs<IOContext>
    {
        using Base = boost::asio::io_context;
    public:
        using ExecutorWorkGuard = boost::asio::executor_work_guard<typename Base::executor_type>;
        IOContext() : Base(), m_work_guard(boost::asio::make_work_guard(*this)) {}
        virtual ~IOContext() {}
        void stop()
        {
            m_work_guard.reset();
            Base::stop();
        }
    private:
        ExecutorWorkGuard m_work_guard;
    };

    class ThreadIOContext : public IOContext, public STDPointerDefs<ThreadIOContext>
    {
        using Base = IOContext;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<ThreadIOContext>);
        ThreadIOContext() : Base(), m_thread([this]() { Base::run(); }) {}
        virtual ~ThreadIOContext()
        {
            Base::stop();
            if (m_thread.joinable()) {
                m_thread.join();
            }
        }
    private:
        std::thread m_thread;
    };

    class Timer
    {
        using Self = Timer;
    public:
        using Strand = boost::asio::strand<IOContext::executor_type>;
        using ClockTimer = boost::asio::steady_timer;
        using Interval = typename ClockTimer::clock_type::duration;
        using Callback = std::function<void()>;
        using Mutex = std::mutex;
        using LockGuard = std::lock_guard<Mutex>;
        using ErrorCode = boost::system::error_code;
        Timer(const IOContext::SharedPointer & io_context_sp) :
            m_io_context_sp(io_context_sp),
            m_strand(boost::asio::make_strand(*m_io_context_sp)),
            m_timer(*m_io_context_sp),
            m_interval(0),
            m_is_running(false) { }
        ~Timer()
        {
            std::promise<void> done;
            auto future = done.get_future();
            boost::asio::post(m_strand, [this, &done] {
                m_is_running = false;
                done.set_value();
            });
            future.wait();
        }
        Self & setInterval(Interval milliseconds)
        {
            m_interval = milliseconds;
            return *this;
        }
        Self & setCallback(Callback callback)
        {
            m_callback = std::move(callback);
            return *this;
        }
        void start()
        {
            if (not m_callback) { return; }
            boost::asio::post(m_strand, [this] {
                if (m_is_running) { return; }
                m_is_running = true;
                this->scheduleNext();
            });
        }
        void stop()
        {
            boost::asio::post(m_strand, [this] {
                m_is_running = false;
                m_timer.cancel();
            });
        }
        void singleShot(Interval milliseconds, Callback callback)
        {
            if (not callback) { return; }
            boost::asio::post(*m_io_context_sp, [this, milliseconds, callback] {
                auto timer = std::make_shared<boost::asio::steady_timer>(*m_io_context_sp);
                timer->expires_after(milliseconds);
                timer->async_wait([callback, timer](const ErrorCode & error_code) {
                    if (error_code) {
                        lcf_log_error("Timer error: {}", error_code.message());
                        return;
                    }
                    callback();
                });
            });
        }
    private:
        void scheduleNext()
        {
            if (not m_is_running) { return; }
            m_timer.expires_after(m_interval);
            m_timer.async_wait(boost::asio::bind_executor(m_strand, [this](const ErrorCode & error_code) {
                if (not m_is_running) { return; }
                if (error_code) {
                    lcf_log_error("Timer error: {}", error_code.message());
                    return;
                }
                m_callback();
                this->scheduleNext();
            }));
        }
    private:
        IOContext::SharedPointer m_io_context_sp;
        Strand m_strand;
        ClockTimer m_timer;
        Callback m_callback;
        Interval m_interval;
        bool m_is_running;
    };
}