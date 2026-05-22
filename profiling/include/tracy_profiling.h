#pragma once

#ifdef USE_TRACY
#include <tracy/Tracy.hpp>

#define TRACY_SCOPE_BEGIN_NC(name, color) { ZoneScopedNC(name, color);
#define TRACY_SCOPE_END() }

// 函数级 zone 宏：放在函数/{} 块开头，作用域 = 当前 {} 块。
// 与 TRACY_SCOPE_BEGIN_NC 不同，不引入额外大括号副作用。
//   LCF_TRACY_ZONE_N("Render::Frame")
//   LCF_TRACY_ZONE_NC("Render::CullCpu", tracy::Color::Yellow2)
#define LCF_TRACY_ZONE_N(name)         ZoneScopedN(name)
#define LCF_TRACY_ZONE_NC(name, color) ZoneScopedNC(name, color)
#define LCF_TRACY_FRAMEMARK            FrameMark
#else
#define TRACY_SCOPE_BEGIN_NC(name, color)
#define TRACY_SCOPE_END()
#define LCF_TRACY_ZONE_N(name)
#define LCF_TRACY_ZONE_NC(name, color)
#define LCF_TRACY_FRAMEMARK
#endif