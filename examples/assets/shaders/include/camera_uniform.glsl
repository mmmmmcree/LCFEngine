layout(set = 0, binding = 0) uniform camera_uniform {
    mat4 projection;
    mat4 view;
    mat4 projection_view;
    vec3 camera_position;
    vec4 frustum_planes[6];
};