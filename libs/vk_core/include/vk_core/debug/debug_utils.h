#pragma once

#include <vulkan/vulkan.hpp>
#include <functional>
#include <string_view>
#include "resource_utils.h"

namespace lcf::vkc::dbg {

using SeverityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT;

using LogSink = std::function<void(std::string_view message)>;

class DebugLogCallbacks
{
    using Self = DebugLogCallbacks;
public:
    DebugLogCallbacks() noexcept;
public:
    Self & setVerboseSink(LogSink sink) noexcept { if (sink) { m_verbose_log_sink = std::move(sink); } return *this; }
    Self & setInfoSink(LogSink sink) noexcept { if (sink) { m_info_log_sink = std::move(sink); } return *this; }
    Self & setWarningSink(LogSink sink) noexcept { if (sink) { m_warning_log_sink = std::move(sink); } return *this; }
    Self & setErrorSink(LogSink sink) noexcept { if (sink) { m_error_log_sink = std::move(sink); } return *this; }
    void logVerbose(std::string_view message) const noexcept { m_verbose_log_sink(message); }
    void logInfo(std::string_view message) const noexcept { m_info_log_sink(message); }
    void logWarning(std::string_view message) const noexcept { m_warning_log_sink(message); }
    void logError(std::string_view message) const noexcept { m_error_log_sink(message); }
private:
    LogSink m_verbose_log_sink;
    LogSink m_info_log_sink;
    LogSink m_warning_log_sink;
    LogSink m_error_log_sink;
};

ResourceLease enable_debug_utils(
    vk::Instance instance,
    vk::DebugUtilsMessageSeverityFlagsEXT severity,
    DebugLogCallbacks callbacks = {}) noexcept;

} // namespace lcf::vkc::dbg
