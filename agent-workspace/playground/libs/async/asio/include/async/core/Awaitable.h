#pragma once

#include <asio/awaitable.hpp>

namespace lcf::async {

template <typename T = void>
using Awaitable = asio::awaitable<T>;

}
