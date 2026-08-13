#pragma once

#include <system_error>
#include <string>
#include <variant>

namespace lcf::vkc {

enum class errc
{
    no_error = 0,
    no_suitable_instance,
    no_suitable_physical_device,
    no_suitable_queue_family,
    preferred_device_type_unavailable,
    no_suitable_surface_format,
    no_suitable_present_mode,
    no_suitable_present_queue_family,
    no_device_queue_requested,
    main_device_role_not_configured,
    surface_zero_size,
    missing_required_instance_extension,
    missing_required_device_extension,
    missing_required_device_feature,
    present_skipped_for_resize,
    command_buffer_batch_exhausted,
    command_buffer_batch_queue_mismatch,
};

enum class warnc
{
    no_warning = 0,
    requested_instance_layer_unavailable,
};

std::error_code make_error_code(errc error) noexcept;

std::error_code make_error_code(warnc warning) noexcept;

class Error
{
    using Self = Error;
public:
    ~Error() noexcept = default;
    Error() noexcept = default;
    Error(std::error_code code) noexcept : m_code(code), m_detail(code.message()) {}
    Error(std::error_code code, std::string detail) noexcept : m_code(code), m_detail(std::move(detail)) {}
    Error(const Self &) = default;
    Error(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
    operator const std::error_code &() const noexcept { return m_code; }
    explicit operator bool() const noexcept { return bool(m_code); }
public:
    const std::error_code & code() const noexcept { return m_code; }
    const std::string & message() const noexcept { return m_detail; }
private:
    std::error_code m_code;
    std::string m_detail;
};

class Warning
{
    using Self = Warning;
public:
    ~Warning() noexcept = default;
    Warning() noexcept = default;
    Warning(std::error_code code) noexcept : m_code(code), m_detail(code.message()) {}
    Warning(std::error_code code, std::string detail) noexcept : m_code(code), m_detail(std::move(detail)) {}
    Warning(const Self &) = default;
    Warning(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
    operator const std::error_code &() const noexcept { return m_code; }
    explicit operator bool() const noexcept { return bool(m_code); }
public:
    const std::error_code & code() const noexcept { return m_code; }
    const std::string & message() const noexcept { return m_detail; }
private:
    std::error_code m_code;
    std::string m_detail;
};

class Failure
{
    struct Monostate
    {
        const std::error_code & code() const noexcept { return m_code; }
        const std::string & message() const noexcept { return m_detail; }
        std::error_code m_code {};
        std::string m_detail {};
    };
    using Diagnostic = std::variant<Monostate, Warning, Error>;
public:
    Failure() noexcept = default;
    Failure(Error error) noexcept : m_diagnostic(std::move(error)) {}
    Failure(Warning warning) noexcept : m_diagnostic(std::move(warning)) {}
    explicit operator bool() const noexcept { return this->isError(); }
public:
    bool isWarning() const noexcept
    {
        const auto * warning = std::get_if<Warning>(&m_diagnostic);
        return warning && bool(*warning);
    }
    bool isError() const noexcept
    {
        const auto * error = std::get_if<Error>(&m_diagnostic);
        return error && bool(*error);
    }
    std::error_code code() const noexcept
    {
        return std::visit([](const auto & diagnostic) { return diagnostic.code(); }, m_diagnostic);
    }
    const std::string & message() const noexcept
    {
        return std::visit([](const auto & diagnostic) -> const std::string & { return diagnostic.message(); }, m_diagnostic);
    }
private:
    Diagnostic m_diagnostic;
};

} // namespace lcf::vkc

template <>
struct std::is_error_code_enum<lcf::vkc::errc> : std::true_type {};

template <>
struct std::is_error_code_enum<lcf::vkc::warnc> : std::true_type {};