#pragma once

#include <boost/asio.hpp>
#include "PointerDefs.h"

namespace lcf {
    class BoostIOContext : public boost::asio::io_context, public STDPointerDefs<BoostIOContext>
    {
        using Base = boost::asio::io_context;
    public:
        using ExecutorWorkGuard = boost::asio::executor_work_guard<typename Base::executor_type>;
        BoostIOContext() : Base(), m_work_guard(boost::asio::make_work_guard(*this)) {}
        void stop()
        {
            m_work_guard.reset();
            Base::stop();
        }
    private:
        ExecutorWorkGuard m_work_guard;
    };

    class BoostThreadIOContext : public BoostIOContext, public STDPointerDefs<BoostThreadIOContext>
    {
        using Base = BoostIOContext;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<BoostThreadIOContext>);
        BoostThreadIOContext() : Base(), m_thread([this]() { Base::run(); }) {}
        ~BoostThreadIOContext()
        {
            Base::stop();
            if (m_thread.joinable()) {
                m_thread.join();
            }
        }
    private:
        std::thread m_thread;
    };

    class ThreadTimer
    {
        using Self = ThreadTimer;
    public:
        using IOContext = BoostThreadIOContext;
        using Timer = boost::asio::steady_timer;
        using Interval = typename Timer::clock_type::duration;
        using Callback = std::function<void()>;
        using Mutex = std::mutex;
        using LockGuard = std::lock_guard<Mutex>;
        ThreadTimer(const IOContext::SharedPointer & io_context_sp = nullptr) :
            m_io_context_sp(io_context_sp? io_context_sp : IOContext::makeShared()),
            m_timer(*m_io_context_sp),
            m_interval(0),
            m_is_running(false) { }
        ~ThreadTimer()
        {
            this->stop();
        }
        Self & setInterval(Interval milliseconds)
        {
            LockGuard lock(m_mutex);
            m_interval = milliseconds;
            return *this;
        }
        Self & setCallback(Callback && callback)
        {
            LockGuard lock(m_mutex);
            m_callback = std::move(callback);
            return *this;
        }
        void start()
        {
            boost::asio::post(*m_io_context_sp, [this] {
                LockGuard lock(m_mutex);
                if (m_is_running) { return; }
                m_is_running = true;
                this->scheduleNext();
            });
        }
        void stop()
        {
            boost::asio::post(*m_io_context_sp, [this] {
                LockGuard lock(m_mutex);
                m_is_running = false;
                m_timer.cancel();
            });
        }
        void singleShot(Interval milliseconds, Callback callback)
        {
            boost::asio::post(*m_io_context_sp, [this, milliseconds, callback] {
                auto timer = std::make_shared<boost::asio::steady_timer>(*m_io_context_sp);
                timer->expires_after(milliseconds);
                timer->async_wait([callback, timer](const boost::system::error_code & error_code) {
                    if (not error_code and callback) {
                        callback();
                    }
                });
            });
        }
    private:
        void scheduleNext()
        {
            if (not m_is_running) { return; }
            m_timer.expires_after(m_interval);
            m_timer.async_wait([this](const boost::system::error_code& error_code) {
                LockGuard lock(m_mutex);
                if (not error_code and m_callback) {
                    m_callback();
                    this->scheduleNext();
                }
            });
        }
    private:
        IOContext::SharedPointer m_io_context_sp;
        Timer m_timer;
        Mutex m_mutex;
        Callback m_callback;
        Interval m_interval;
        bool m_is_running;
    };
}