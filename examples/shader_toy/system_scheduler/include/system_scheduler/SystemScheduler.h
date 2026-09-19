#pragma once

#include <system_error>

namespace lcf::shader_toy {

class SystemScheduler
{
    using Self = SystemScheduler;
public:
    ~SystemScheduler() noexcept = default;
    SystemScheduler() noexcept = default;
    SystemScheduler(const Self &) = delete;
    SystemScheduler(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    std::error_code tick() noexcept;
};

} // namespace lcf::shader_toy
