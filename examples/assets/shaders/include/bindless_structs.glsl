struct DrawMetaInfo
{
// vk::DrawIndirectCommand
    uint vertex_count;
    uint instance_count;
    uint first_vertex;
    uint first_instance;
// custom
    uint object_id;
};

struct ObjectInfo
{
    uint64_t vertex_buffer;
    uint64_t index_buffer;
    uint64_t material_params;
    uint64_t material_texture_ids;
    vec4 bounding_sphere;
};

struct InstanceInfo
{
    mat4 transform;
};