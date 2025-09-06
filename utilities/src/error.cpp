#include "error.h"
#include <iostream>
#include <format>

void lcf::throw_runtime_error(const std::string &msg, const char *file, int line)
{
    std::string error_msg = std::format("Runtime Error: {} {:->10}At file {}: line {}\n", msg, ' ', file, line);
    std::cerr << error_msg;
    throw std::runtime_error(error_msg);
}