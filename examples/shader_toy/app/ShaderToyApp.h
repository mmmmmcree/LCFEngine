#pragma once

#include <memory>
#include <system_error>

namespace lcf::shader_toy {

class App
{
    using Self = App;
    struct Impl;
public:
    ~App() noexcept;
    App() noexcept;
    App(const Self &) = delete;
    App(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    std::error_code create() noexcept;
    std::error_code run() noexcept;
private:
    std::unique_ptr<Impl> m_impl_up;
};

} // namespace lcf::shader_toy
