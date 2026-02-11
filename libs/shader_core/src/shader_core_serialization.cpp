#pragma once

#include "shader_core/shader_core_serialization.h"
#include "shader_core/ShaderResource.h"
#include "enums/enum_name.h"
#include <ranges>

using namespace lcf;
namespace stdr = std::ranges;
namespace stdv = std::views;

OrderedJSON lcf::serialize(const ShaderResourceMember &shader_resource_member)
{
    OrderedJSON json;
    json["name"] = shader_resource_member.getName();
    json["base_type"] = enum_name(shader_resource_member.getBaseDataType());
    json["width(bits)"] = shader_resource_member.getWidth();
    json["vecsize"] = shader_resource_member.getVecSize();
    json["columns"] = shader_resource_member.getColumns();
    json["array_size"] = shader_resource_member.getArraySize();
    json["offset"] = shader_resource_member.getOffset();
    json["size(bytes)"] = shader_resource_member.getSizeInBytes();
    if (not shader_resource_member.getMembers().empty()) {
        OrderedJSON members_array = OrderedJSON::array();
        for (const auto& nested_member : shader_resource_member.getMembers()) {
            members_array.push_back(serialize(nested_member));
        }
        json["members"] = members_array;
    }
    return json;
}

OrderedJSON lcf::serialize(const ShaderResource &shader_resource)
{
    OrderedJSON json;
    json["type_name"] = "ShaderResource";
    json["name"] = shader_resource.getName();
    json["location"] = shader_resource.getLocation();
    json["binding"] = shader_resource.getBinding();
    json["set"] = shader_resource.getSet();
    json.update(serialize(static_cast<const ShaderResourceMember&>(shader_resource)));
    return json;
}

OrderedJSON serialize_resource_list(const ShaderResourceList& list)
{
    OrderedJSON array = OrderedJSON::array();
    for (const auto& resource : list) {
        array.push_back(serialize(resource));
    }
    return array;
};

OrderedJSON lcf::serialize(const ShaderResources &shader_resources)
{
    
    OrderedJSON json;
    if (not shader_resources.uniform_buffers.empty()) {
        json["uniform_buffers"] = serialize_resource_list(shader_resources.uniform_buffers);
    }
    if (not shader_resources.storage_buffers.empty()) {
        json["storage_buffers"] = serialize_resource_list(shader_resources.storage_buffers);
    }
    if (not shader_resources.stage_inputs.empty()) {
        json["stage_inputs"] = serialize_resource_list(shader_resources.stage_inputs);
    }
    if (not shader_resources.stage_outputs.empty()) {
        json["stage_outputs"] = serialize_resource_list(shader_resources.stage_outputs);
    }
    if (not shader_resources.subpass_inputs.empty()) {
        json["subpass_inputs"] = serialize_resource_list(shader_resources.subpass_inputs);
    }
    if (not shader_resources.storage_images.empty()) {
        json["storage_images"] = serialize_resource_list(shader_resources.storage_images);
    }
    if (not shader_resources.sampled_images.empty()) {
        json["sampled_images"] = serialize_resource_list(shader_resources.sampled_images);
    }
    if (not shader_resources.atomic_counters.empty()) {
        json["atomic_counters"] = serialize_resource_list(shader_resources.atomic_counters);
    }
    if (not shader_resources.acceleration_structures.empty()) {
        json["acceleration_structures"] = serialize_resource_list(shader_resources.acceleration_structures);
    }
    if (not shader_resources.gl_plain_uniforms.empty()) {
        json["gl_plain_uniforms"] = serialize_resource_list(shader_resources.gl_plain_uniforms);
    }
    if (not shader_resources.push_constant_buffers.empty()) {
        json["push_constant_buffers"] = serialize_resource_list(shader_resources.push_constant_buffers);
    }
    if (not shader_resources.shader_record_buffers.empty()) {
        json["shader_record_buffers"] = serialize_resource_list(shader_resources.shader_record_buffers);
    }
    if (not shader_resources.separate_images.empty()) {
        json["separate_images"] = serialize_resource_list(shader_resources.separate_images);
    }
    if (not shader_resources.separate_samplers.empty()) {
        json["separate_samplers"] = serialize_resource_list(shader_resources.separate_samplers);
    }
    if (not shader_resources.builtin_inputs.empty()) {
        json["builtin_inputs"] = serialize_resource_list(shader_resources.builtin_inputs);
    }
    if (not shader_resources.builtin_outputs.empty()) {
        json["builtin_outputs"] = serialize_resource_list(shader_resources.builtin_outputs);
    }
    return json;
}
