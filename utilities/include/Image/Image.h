#pragma once
#include <string_view>

namespace lcf {
    class Image
    {
    public:
        Image(std::string_view file_path, int requested_channels = 0, bool flip_y = false);
        ~Image();
        operator bool() const;
        const void *getData() const;
        void *data();
        template <typename T>
        const T *data() const { return static_cast<const T *>(m_data); }
        bool create();
        int getWidth() const;
        int getHeight() const;
        int getChannels() const;
        size_t getSizeInBytes() const;
        void saveAsHDR(const char *filename) const;
    private:
        int m_width = 0;
        int m_height = 0;
        int m_channels = 0;
        void *m_data = nullptr;
    };
}