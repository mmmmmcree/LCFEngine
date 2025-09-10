#pragma once
#include <stdint.h>

namespace lcf::render {
    class GPUBuffer
    {
    public:
        enum class UsagePattern
        {
            eDynamic,
            eStatic,
        };
        GPUBuffer() = default;
        virtual ~GPUBuffer() = default;
        uint32_t getSize() const { return m_size; }
        void setUsagePattern(UsagePattern usage_pattern) { m_usage_pattern = usage_pattern; }
        UsagePattern getUsagePattern() const { return m_usage_pattern; }
    protected:
        uint32_t m_size = 0;
        UsagePattern m_usage_pattern = UsagePattern::eDynamic;
    };
}