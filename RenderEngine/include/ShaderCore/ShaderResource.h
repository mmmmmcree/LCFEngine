#pragma once

#include "ShaderDefs.h"
#include "JSON.h"
#include <vector>
#include <unordered_map>
#include <map>

namespace lcf {
    class ShaderResourceMember
    {
        using Self = ShaderResourceMember;
    public:
        using MemberList = std::vector<ShaderResourceMember>;
        Self & setName(std::string_view name) { m_name = name; return *this; }
        Self & setBaseDataType(ShaderDataType base_type) { m_base_type = base_type; return *this; }
        Self & setWidth(uint32_t width) { m_width = width; return *this; }
        Self & setVecSize(uint32_t vecsize) { m_vecsize = vecsize; return *this; }
        Self & setColumns(uint32_t columns) { m_columns = columns; return *this; }
        Self & setOffset(uint32_t offset) { m_offset = offset; return *this; }
        Self & setArraySize(uint32_t array_size) { m_array_size = std::max(1u, array_size); return *this; }
        Self & addMember(const ShaderResourceMember &member) { m_members.emplace_back(member); return *this; }
        Self & setSizeInBytes(uint32_t size) { m_size = size; return *this; }
        ShaderDataType getBaseDataType() const { return m_base_type; }
        const std::string & getName() const { return m_name; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getVecSize() const { return m_vecsize; }
        uint32_t getColumns() const { return m_columns; }
        uint32_t getArraySize() const { return m_array_size; }
        uint32_t getOffset() const { return m_offset; }
        uint32_t getSizeInBytes() const { return m_size; }
        MemberList & getMembers() { return m_members; }
        const MemberList & getMembers() const { return m_members; }
    private:
        std::string m_name;
        ShaderDataType m_base_type = ShaderDataType::Unknown;
        uint32_t m_width = 0;
        uint32_t m_vecsize = 1;
        uint32_t m_columns = 1;
        uint32_t m_array_size = 1;
        uint32_t m_offset = 0;
        uint32_t m_size = 0;
        MemberList m_members;
    };

    class ShaderResource : public ShaderResourceMember
    {
        using Self = ShaderResource;
    public:
        ShaderResource() = default;
        // Self & setName(std::string_view name) { m_name = name; return *this; }
        Self & setLocation(uint32_t location) { m_location = location; return *this; }
        Self & setBinding(uint32_t binding) { m_binding = binding; return *this; }
        Self & setSet(uint32_t set) { m_set = set; return *this; }
        // const std::string &getName() const { return m_name; }
        bool hasLocation() const { return m_location != -1; }
        bool hasBinding() const { return m_binding != -1; }
        bool hasSet() const { return m_set != -1; }
        uint32_t getLocation() const { return m_location; }
        uint32_t getBinding() const { return m_binding; }
        uint32_t getSet() const { return m_set; }
    private:
        // std::string m_name;
        uint32_t m_location = 0;
        uint32_t m_binding = 0;
        uint32_t m_set = 0;
    };

    using NameToShaderResourceMap = std::unordered_map<std::string, ShaderResource>;
    using LocationToShaderResourceMap = std::map<uint32_t, ShaderResource>;
    using ShaderResourceList = std::vector<ShaderResource>;

    struct PushConstantBuffer
    {
        
    };

    struct ShaderResources
    {
        ShaderResourceList uniform_buffers;
        ShaderResourceList storage_buffers;
        ShaderResourceList stage_inputs;
        ShaderResourceList stage_outputs;
        ShaderResourceList subpass_inputs;
        ShaderResourceList storage_images;
        ShaderResourceList sampled_images;
        ShaderResourceList atomic_counters;
        ShaderResourceList acceleration_structures;
        ShaderResourceList gl_plain_uniforms;
        ShaderResourceList push_constant_buffers;
        ShaderResourceList shader_record_buffers;
        ShaderResourceList separate_images;
        ShaderResourceList separate_samplers;
        ShaderResourceList builtin_inputs;
        ShaderResourceList builtin_outputs;
    };

    OrderedJSON serialize(const ShaderResourceMember & shader_resource_member);

    OrderedJSON serialize(const ShaderResource & shader_resource);

    OrderedJSON serialize(const ShaderResources & shader_resources);
}