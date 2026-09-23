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
    if (m_is_watching) { return std::make_error_code(std::errc::operation_not_permitted); }
    if (not std::filesystem::exists(directory)) { return std::make_error_code(std::errc::no_such_file_or_directory); }
    const efsw::WatchID watch_id = m_watcher.addWatch(directory.string(), this, recursive);
    if (watch_id < 0) { return std::make_error_code(std::errc::io_error); }
    m_watch_ids.push_back(watch_id);
    return {};
}

std::error_code FileWatcher::watch() noexcept
{
    if (m_is_watching) { return std::make_error_code(std::errc::operation_in_progress); }
    if (m_watch_ids.empty()) { return std::make_error_code(std::errc::invalid_argument); }
    try {
        m_watcher.watch();
        m_is_watching = true;
    } catch (const std::system_error & e) {
        return e.code();
    } catch (...) {
        return std::make_error_code(std::errc::io_error);
    }
    return {};
}

std::vector<FileAction> FileWatcher::pollEvents() noexcept
{
    std::vector<FileAction> actions;
    actions.reserve(m_actions.size_approx());
    FileAction action;
    while (m_actions.try_dequeue(action)) {
        actions.emplace_back(std::move(action));
    }
    return actions;
}

void FileWatcher::handleFileAction(
    efsw::WatchID,
    const std::string & directory,
    const std::string & filename,
    efsw::Action action,
    const std::string & old_filename
)
{
    const std::filesystem::path path = std::filesystem::path(directory) / filename;
    switch (action) {
        case efsw::Actions::Add: { m_actions.enqueue(FileAdd {std::move(path)}); } break;
        case efsw::Actions::Delete: { m_actions.enqueue(FileDelete {std::move(path)}); } break;
        case efsw::Actions::Modified: { m_actions.enqueue(FileModify {std::move(path)}); } break;
        case efsw::Actions::Moved: { m_actions.enqueue(FileMove{std::move(path), std::filesystem::path(directory) / old_filename }); } break;
        default: { std::unreachable(); }
    }
}

} // namespace lcf::shader_toy
