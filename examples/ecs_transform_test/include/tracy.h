#pragma once

// why is TRACY_ENABLE auto defined?
#include <tracy/Tracy.hpp>

#define TRACY_SCOPE_BEGIN_NC(name, color) { ZoneScopedNC(name, color);
#define TRACY_SCOPE_END() }