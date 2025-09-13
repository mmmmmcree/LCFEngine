#include <stdint.h>

namespace lcf::render {
    // enum class GPUBufferPattern;

    // class GPUBuffer2
    // {
    // public:
    //     GPUBuffer2() = default;
    //     virtual ~GPUBuffer2() = default;
    //     uint32_t getSize() const { return m_size; }
    //     GPUBuffer2 & setUsagePattern(GPUBufferPattern pattern) { m_pattern = pattern; return *this; }
    //     GPUBufferPattern getUsagePattern() const { return m_pattern; }
    // protected:
    //     uint32_t m_size = 0;
    //     GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
    // };

    enum class GPUBufferPattern
    {
        eDynamic, // frequently update
        eStatic,  // rarely update
    };

    enum class GPUBufferUsage : uint8_t {
        eVertex,
        eIndex,
        eUniform,
        eShaderStorage,
        eStaging,
    };
}