#pragma once
#include <string_view>
#include <span>

namespace lcf {
    class Image
    {
    public:
        Image(std::string_view file_path, int requested_channels = 0, bool flip_y = false);
        ~Image();
        operator bool() const;
        std::span<const std::byte> getDataSpan() const { return {static_cast<const std::byte*>(m_data), getSizeInBytes()}; }
        bool create();
        uint32_t getWidth() const noexcept;
        uint32_t getHeight() const noexcept;
        uint32_t getChannels() const noexcept;
        size_t getSizeInBytes() const noexcept;
        void saveAsHDR(const char *filename) const;
    private:
        int m_width = 0;
        int m_height = 0;
        int m_channels = 0;
        void * m_data = nullptr;
    };
}