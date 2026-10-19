module;

#include <async/dependencies.h>

export module lcf.async;

#define LCF_ASYNC_BUILDING_MODULE
#define LCF_ASYNC_EXPORT export
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5244)
#endif
#include <async/implementation.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
