#pragma once

#include <functional>
#include <tsl/robin_map.h>

namespace lcf::shader_toy {

template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using RobinMap = tsl::robin_map<Key, T, Hash, KeyEqual>;

} // namespace lcf::shader_toy
