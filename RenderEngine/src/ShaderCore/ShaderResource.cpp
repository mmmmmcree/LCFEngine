#include "ShaderResource.h"
#include <format>
#include <sstream>


std::string lcf::ShaderResourceMember::toString() const
{
    std::string str = std::format(
        "  base_type: {}\n"
        "  width: {}\n"
        "  vecsize: {}\n"
        "  columns: {}\n"
        "  array_size: {}\n"
        "  offset: {}\n"
        "  size: {}",
        static_cast<int>(m_base_type), m_width, m_vecsize, m_columns, m_array_size, m_offset, m_size);
    for (int i = 0; i < m_members.size(); ++i) {
        const auto &member = m_members[i];
        std::string member_str = "member[" + std::to_string(i) + "]\n" + member.toString();
        std::istringstream stream(member_str);
        std::string line;
        while (std::getline(stream, line)) { str += "\n  " + line; }
    }
    return str;
}

std::string lcf::ShaderResource::toString() const
{
    std::string str = std::format(
        "lcf::ShaderResource2\n"
        "  name: {}\n"
        "  location: {}\n"
        "  binding: {}\n"
        "  set: {}",
        m_name, m_location, m_binding, m_set);
    std::string member_str = ShaderResourceMember::toString();
    std::istringstream stream(member_str);
    std::string line;
    while (std::getline(stream, line)) { str += "\n" + line; }
    return str;
}