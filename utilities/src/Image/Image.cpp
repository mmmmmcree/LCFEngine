#include "Image.h"
#include <filesystem>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
 

lcf::Image::Image(std::string_view file_path, int requested_channels, bool flip_y)
{
    if (not std::filesystem::exists(file_path)) {
        std::cerr << "lcf::Image: file not found: " << file_path << std::endl;
        return;
    }
    stbi_set_flip_vertically_on_load(flip_y);
    if (stbi_is_hdr(file_path.data())) {
        m_data = stbi_loadf(file_path.data(), &m_width, &m_height, &m_channels, requested_channels);
    } else {
        m_data = stbi_load(file_path.data(), &m_width, &m_height, &m_channels, requested_channels);
    }
    stbi_set_flip_vertically_on_load(false);
    m_channels = std::max(m_channels, requested_channels);
}

lcf::Image::~Image()
{
    stbi_image_free(m_data);
    m_data = nullptr;
}

lcf::Image::operator bool() const
{
    return m_data;
}

int lcf::Image::getWidth() const
{
    return m_width;
}

int lcf::Image::getHeight() const
{
    return m_height;
}

int lcf::Image::getChannels() const
{
    return m_channels;
}

size_t lcf::Image::getSizeInBytes() const
{
    return m_width * m_height * m_channels * sizeof(uint8_t);
}

void lcf::Image::saveAsHDR(const char *filename) const
{
    stbi_write_hdr(filename, m_width, m_height, m_channels, static_cast<const float *>(m_data));
}
