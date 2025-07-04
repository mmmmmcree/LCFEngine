#pragma once

#include "InterleavedSpan.h"

namespace lcf {
    class InterleavedBuffer : public lcf::InterleavedSpan
    {
        using Base = lcf::InterleavedSpan;
    public:
        InterleavedBuffer(size_t size, std::span<const size_t> attr_sizes_in_bytes)
        {
            // 这里可以根据对齐规则调整偏移量
            this->setLayout(attr_sizes_in_bytes);
            m_data.resize(size * this->getStrideInBytes());
            this->bind(m_data);
        }
        InterleavedBuffer(size_t size, std::initializer_list<size_t> attr_sizes_in_bytes) :
            InterleavedBuffer(size, std::span<const size_t>(attr_sizes_in_bytes)) { }
    private:
        using Base::bind;
    private:
        std::vector<std::byte> m_data;
    };
}