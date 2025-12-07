#pragma once

#include <string>

namespace lcf {
    class IUserCommandContext
    {
    public:
        virtual ~IUserCommandContext() = default;
        virtual void execute(const std::string & command_line) noexcept = 0;
        virtual bool isActive() const noexcept = 0;
    };
}