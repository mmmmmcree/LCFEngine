#include "FileWatcher.h"

namespace lcf::shader_toy {

FileWatcher::~FileWatcher() noexcept
{
    for (const efsw::WatchID watch_id : m_watch_ids) {
        m_watcher.removeWatch(watch_id);
    }
}

std::error_code FileWatcher::addWatchedDirectory(const std::filesystem::path & directory, bool recursive) noexcept
{
    if (not std::filesystem::exists(directory)) {
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }
    const efsw::WatchID watch_id = m_watcher.addWatch(directory.string(), this, recursive);
    if (watch_id < 0) { return std::make_error_code(std::errc::io_error); }
    m_watch_ids.push_back(watch_id);
    if (not m_is_watching) {
        m_watcher.watch();
        m_is_watching = true;
    }
    return {};
}

void FileWatcher::handleFileAction(
    efsw::WatchID,
    const std::string & directory,
    const std::string & filename,
    efsw::Action action,
    const std::string & old_filename
)
{
    std::filesystem::path path = std::filesystem::path(directory) / filename;
    switch (action) {
        case efsw::Actions::Add: { m_on_action(FileAdd{std::move(path)}); } break;
        case efsw::Actions::Delete: { m_on_action(FileDelete{std::move(path)}); } break;
        case efsw::Actions::Modified: { m_on_action(FileModify{std::move(path)}); } break;
        case efsw::Actions::Moved: { m_on_action(FileMove{std::move(path), std::filesystem::path(directory) / old_filename}); } break;
        default: { std::unreachable(); }
    }
}
} // namespace lcf::shader_toy
