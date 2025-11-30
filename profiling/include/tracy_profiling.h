#pragma once

#ifdef USE_TRACY
#include <tracy/Tracy.hpp>

#define TRACY_SCOPE_BEGIN_NC(name, color) { ZoneScopedNC(name, color);
#define TRACY_SCOPE_END() }
#else
#define TRACY_SCOPE_BEGIN_NC(name, color)
#define TRACY_SCOPE_END()
#endif