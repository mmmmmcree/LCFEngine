#pragma once
#include <stdint.h>

namespace lcf::render {
    class GPUBuffer
    {
    public:
        enum class UsagePattern
        {
            Dynamic,
            Static,
        };
        GPUBuffer() = default;
        virtual ~GPUBuffer() = default;
        virtual void resize(uint32_t size) = 0;
        virtual void setData(const void *data, uint32_t size) = 0;
        virtual void setSubData(const void *data, uint32_t size, uint32_t offset) = 0;
        virtual bool create() = 0;
        virtual bool isCreated() const = 0;
        uint32_t getSize() const { return m_size; }
        void setUsagePattern(UsagePattern usage_pattern) { m_usage_pattern = usage_pattern; }
        UsagePattern getUsagePattern() const { return m_usage_pattern; }
    protected:
        uint32_t m_size = 0;
        UsagePattern m_usage_pattern = UsagePattern::Dynamic;
    };
}