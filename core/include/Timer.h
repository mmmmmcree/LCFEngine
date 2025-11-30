#pragma once

#include <boost/asio.hpp>
#include "PointerDefs.h"
#include "log.h"

namespace lcf {
    class IOContext : public boost::asio::io_context, public STDPointerDefs<IOContext>
    {
        using Base = boost::asio::io_context;
    public:
        IOContext() : Base() {}
        virtual ~IOContext() {}
    };

    class ThreadIOContext : public IOContext, public STDPointerDefs<ThreadIOContext>
    {
        using Base = IOContext;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<ThreadIOContext>);
        using ExecutorWorkGuard = boost::asio::executor_work_guard<typename Base::executor_type>;
        ThreadIOContext() :
            Base(),
            m_work_guard(boost::asio::make_work_guard(*this))
        {}
        void run()
        {
            m_thread = std::thread([this]() { Base::run(); });
        }
        void stop()
        {
            Base::stop();
            if (m_thread.joinable()) {
                m_thread.join();
            }
        }
        virtual ~ThreadIOContext()
        {
            this->stop();
        }
    private:
        std::thread m_thread;
        ExecutorWorkGuard m_work_guard;
    };

    class Timer
    {
        using Self = Timer;
    public:
        using Strand = boost::asio::strand<IOContext::executor_type>;
        using ClockTimer = boost::asio::steady_timer;
        using Interval = typename ClockTimer::clock_type::duration;
        using Callback = std::function<void()>;
        using StopCondition = std::function<bool()>;
        using ErrorCode = boost::system::error_code;
        Timer(const IOContext::SharedPointer & io_context_sp) :
            m_io_context_sp(io_context_sp),
            m_strand(boost::asio::make_strand(*m_io_context_sp)),
            m_timer(*m_io_context_sp),
            m_interval(0),
            m_is_running(false) { }
        ~Timer()
        {
            this->stop();
        }
        Self & setInterval(Interval milliseconds) noexcept { m_interval = milliseconds; return *this; }
        Self & setCallback(Callback callback) noexcept { m_callback = std::move(callback); return *this; }
        void start()
        {
            if (not m_callback) { return; }
            boost::asio::post(m_strand, [this] {
                if (m_is_running) { return; }
                m_is_running = true;
                this->scheduleNext();
            });
        }
        void startUntil(StopCondition stop_condition, Callback on_complete = nullptr)
        {
            m_stop_condition = std::move(stop_condition);
            m_on_complete_callback = std::move(on_complete);
            this->start();
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
                timer->async_wait([callback = std::move(callback), timer](const ErrorCode & error_code) {
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
            if (not m_is_running) {
                this->stop();
                return;
            }
            m_timer.expires_after(m_interval);
            m_timer.async_wait(boost::asio::bind_executor(m_strand, [this](const ErrorCode & error_code) {
                if (not m_is_running) { return; }
                if (error_code) {
                    lcf_log_error("Timer error: {}", error_code.message());
                    return;
                }
                m_callback();
                if (m_stop_condition and m_stop_condition()) {
                    m_is_running = false;
                    if (m_on_complete_callback) {
                        m_on_complete_callback();
                    }
                } 
                this->scheduleNext();
            }));
        }
    private:
        IOContext::SharedPointer m_io_context_sp;
        Strand m_strand;
        ClockTimer m_timer;
        Callback m_callback;
        StopCondition m_stop_condition;
        Callback m_on_complete_callback;
        Interval m_interval;
        bool m_is_running;
    };
}