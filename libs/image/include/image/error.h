#pragma once

#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

namespace lcf::img {

enum class errc
{
    no_error = 0,
    invalid_file_type,
    file_not_found,
    invalid_image,
    unsupported_format,
};

std::error_code make_error_code(errc error) noexcept;

class Error
{
    using Self = Error;
public:
    ~Error() noexcept = default;
    Error() noexcept = default;
    Error(std::error_code code) noexcept : m_code(code), m_detail(code.message()) {}
    Error(std::error_code code, std::string detail) : m_code(code), m_detail(std::move(detail)) {}
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

} // namespace lcf::img

template <>
struct std::is_error_code_enum<lcf::img::errc> : std::true_type {};
