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

lcf::Image::Image(int width, int height, int channels, void *data) :
    m_width(width),
    m_height(height),
    m_channels(channels)
{
    size_t size_in_bytes = width * height * channels;
    m_data = malloc(size_in_bytes);
    if (data) {
        memcpy(m_data, data, size_in_bytes);
    }
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

uint32_t lcf::Image::getWidth() const noexcept
{
    return static_cast<uint32_t>(m_width);
}

uint32_t lcf::Image::getHeight() const noexcept
{
    return static_cast<uint32_t>(m_height);
}

uint32_t lcf::Image::getChannels() const noexcept
{
    return static_cast<uint32_t>(m_channels);
}

size_t lcf::Image::getSizeInBytes() const noexcept
{
    return m_width * m_height * m_channels * sizeof(uint8_t);
}

void lcf::Image::saveAsHDR(const char *filename) const
{
    stbi_write_hdr(filename, m_width, m_height, m_channels, static_cast<const float *>(m_data));
}

void lcf::Image::saveAsJPG(const char *filename, int quality) const
{
    stbi_write_jpg(filename, m_width, m_height, m_channels, m_data, quality);
}