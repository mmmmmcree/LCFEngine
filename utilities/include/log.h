#pragma once

#include "spdlog/spdlog.h"
#ifndef NDEBUG
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <filesystem>
#endif

namespace lcf {
    class Logger
    {
    public:
        static void init()
        {
    #ifndef NDEBUG
            initDebug();
    #else
            initRelease();
    #endif
        }
    private:
    #ifndef NDEBUG
        static void initDebug()
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_color_mode(spdlog::color_mode::always);
            console_sink->set_pattern(
                "\033[35m[%Y-%m-%d\033[0m \033[95m%H:%M:%S.%e]\033[0m "
                "\033[93m[%s\033[0m: \033[96mline %#]\033[0m "
                "\033[94m%!()\033[0m "
                "%^[%l]\n%v%$"
            );
            std::filesystem::create_directories("logs");
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/debug.log", true);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%s:%# %!()] [%l] %v");
            auto logger = std::make_shared<spdlog::logger>(
                "multi_sink",
                spdlog::sinks_init_list{console_sink, file_sink}
            );
            logger->set_level(spdlog::level::trace);
            logger->flush_on(spdlog::level::debug);
            spdlog::set_default_logger(logger);
        }
    #else
        static void initRelease()
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_color_mode(spdlog::color_mode::always);
            console_sink->set_pattern("%^[%l]%$ [%H:%M:%S] \n%v");
            auto logger = std::make_shared<spdlog::logger>("release", console_sink);
            logger->set_level(spdlog::level::info);
            logger->flush_on(spdlog::level::warn);
            spdlog::set_default_logger(logger);
        }
    #endif
    };
}

#ifndef NDEBUG
// Debug 模式：完整功能，编译所有日志
#define lcf_log_trace(...)    SPDLOG_TRACE(__VA_ARGS__)
#define lcf_log_debug(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define lcf_log_info(...)     SPDLOG_INFO(__VA_ARGS__)
#define lcf_log_warn(...)     SPDLOG_WARN(__VA_ARGS__)
#define lcf_log_error(...)    SPDLOG_ERROR(__VA_ARGS__)
#define lcf_log_critical(...) SPDLOG_CRITICAL(__VA_ARGS__)
#else
#define lcf_log_trace(...)
#define lcf_log_debug(...)
#define lcf_log_info(...)     SPDLOG_INFO(__VA_ARGS__)
#define lcf_log_warn(...)     SPDLOG_WARN(__VA_ARGS__)
#define lcf_log_error(...)    SPDLOG_ERROR(__VA_ARGS__)
#define lcf_log_critical(...) SPDLOG_CRITICAL(__VA_ARGS__)
#endif