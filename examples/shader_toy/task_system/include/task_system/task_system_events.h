#pragma once

#include "event/Event.h"
#include <cstdint>
#include <filesystem>

namespace lcf::shader_toy {

inline constexpr std::uint16_t k_task_system_id = 2;

struct FileModifiedPayload
{
    static constexpr std::uint16_t id_v{0};
    std::filesystem::path path;
};

struct FileAddedPayload
{
    static constexpr std::uint16_t id_v{1};
    std::filesystem::path path;
};

struct FileDeletedPayload
{
    static constexpr std::uint16_t id_v{2};
    std::filesystem::path path;
};

struct FileMovedPayload
{
    static constexpr std::uint16_t id_v{3};
    std::filesystem::path path;
    std::filesystem::path old_path;
};

struct WatchDirectoryPayload
{
    static constexpr std::uint16_t id_v{4};
    std::filesystem::path path;
    bool recursive = true;
};

using WatchDirectoryEvent = Event<WatchDirectoryPayload, k_task_system_id, EventState::eRequest>;
using FileModifiedEvent = Event<FileModifiedPayload, k_task_system_id, EventState::eNone>;
using FileAddedEvent = Event<FileAddedPayload, k_task_system_id, EventState::eNone>;
using FileDeletedEvent = Event<FileDeletedPayload, k_task_system_id, EventState::eNone>;
using FileMovedEvent = Event<FileMovedPayload, k_task_system_id, EventState::eNone>;

} // namespace lcf::shader_toy
