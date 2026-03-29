#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <expected>
#include <system_error>

namespace lcf {
    inline std::expected<std::string, std::error_code> read_file_as_string(const std::filesystem::path& path)
    {
        if (not std::filesystem::exists(path)) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }
        std::ifstream file(path, std::ios::in);
        if (not file.is_open()) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        return std::string(std::istreambuf_iterator<char>(file), {});
    }
} 