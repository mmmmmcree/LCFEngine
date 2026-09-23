#pragma once

#include <efsw/efsw.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace lcf::shader_toy {

struct FileAdd { std::filesystem::path path; };
struct FileDelete { std::filesystem::path path; };
struct FileModify { std::filesystem::path path; };
struct FileMove { std::filesystem::path path; std::filesystem::path old_path; };

using FileAction = std::variant<FileAdd, FileDelete, FileModify, FileMove>;

class FileWatcher : private efsw::FileWatchListener
{
public:
    ~FileWatcher() noexcept override;
    explicit FileWatcher(std::function<void(FileAction)> on_action) : m_on_action(std::move(on_action)) {}
    FileWatcher(const FileWatcher &) = delete;
    FileWatcher & operator=(const FileWatcher &) = delete;
public:
    std::error_code addWatchedDirectory(const std::filesystem::path & directory, bool recursive) noexcept;
private:
    void handleFileAction(
        efsw::WatchID watch_id,
        const std::string & directory,
        const std::string & filename,
        efsw::Action action,
        const std::string & old_filename
    ) override;
private:
    std::vector<efsw::WatchID> m_watch_ids;
    std::function<void(FileAction)> m_on_action;
    bool m_is_watching = false;
    efsw::FileWatcher m_watcher;
};

} // namespace lcf::shader_toy
