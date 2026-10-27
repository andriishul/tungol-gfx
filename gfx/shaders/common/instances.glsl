#ifndef TUNGOL_COMMON_INSTANCES_GLSL
#define TUNGOL_COMMON_INSTANCES_GLSL

struct InstanceData
{
    mat4 model;
    uint materialIndex;
    uint boneOffset;
};

layout(set = 1, binding = 0) readonly buffer Instances { InstanceData instances[]; };

#endif
