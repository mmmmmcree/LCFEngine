#pragma once

#include <efsw/efsw.hpp>
#include <filesystem>
#include <readerwriterqueue.h>
#include <system_error>
#include <variant>
#include <vector>

namespace lcf::shader_toy {

struct FileAdd
{
    FileAdd() = default;
    FileAdd(std::filesystem::path path) noexcept : m_path(std::move(path)) {}
    std::filesystem::path m_path;
};

struct FileDelete
{
    FileDelete() = default;
    FileDelete(std::filesystem::path path) noexcept : m_path(std::move(path)) {}
    std::filesystem::path m_path;
};

struct FileModify
{
    FileModify() = default;
    FileModify(std::filesystem::path path) noexcept : m_path(std::move(path)) {}
    std::filesystem::path m_path;
};

struct FileMove
{
    FileMove() = default;
    FileMove(std::filesystem::path path, std::filesystem::path old_path) noexcept :
        m_path(std::move(path)), m_old_path(std::move(old_path)) {}
    std::filesystem::path m_path;
    std::filesystem::path m_old_path;
};

using FileAction = std::variant<FileAdd, FileDelete, FileModify, FileMove>;

class FileWatcher : private efsw::FileWatchListener
{
    using Self = FileWatcher;
    using WatchIdList = std::vector<efsw::WatchID>;
    using ActionQueue = moodycamel::ReaderWriterQueue<FileAction>;
public:
    ~FileWatcher() noexcept override;
    FileWatcher() noexcept = default;
    FileWatcher(const Self &) = delete;
    FileWatcher(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    std::error_code addWatchedDirectory(const std::filesystem::path & directory, bool recursive = true) noexcept;
    std::error_code watch() noexcept;
    std::vector<FileAction> pollEvents() noexcept;
private:
    void handleFileAction(
        efsw::WatchID watch_id,
        const std::string & directory,
        const std::string & filename,
        efsw::Action action,
        const std::string & old_filename
    ) override;
private:
    efsw::FileWatcher m_watcher;
    WatchIdList m_watch_ids;
    ActionQueue m_actions;
    bool m_is_watching = false;
};

} // namespace lcf::shader_toy
