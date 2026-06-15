#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <expected>
#include <system_error>
#include <span>

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

    inline std::expected<std::vector<std::byte>, std::error_code> read_file_as_bytes(const std::filesystem::path& path)
    {
        if (not std::filesystem::exists(path)) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (not file.is_open()) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        auto size = static_cast<size_t>(file.tellg());
        std::vector<std::byte> buffer(size);
        file.seekg(0);
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(size));
        if (not file.good()) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        return buffer;
    }

    inline std::error_code write_file(const std::filesystem::path& path, std::span<const std::byte> data)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (not file.is_open()) {
            return std::make_error_code(std::errc::io_error);
        }
        file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
        if (not file.good()) {
            return std::make_error_code(std::errc::io_error);
        }
        return {};
    }
} 