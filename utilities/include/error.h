#pragma once

#include <string>

namespace lcf {
    void throw_runtime_error(const std::string & msg, const char* file, int line);
}

#define LCF_THROW_RUNTIME_ERROR(msg) lcf::throw_runtime_error(msg, __FILE__, __LINE__)