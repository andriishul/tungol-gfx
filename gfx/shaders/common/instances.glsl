#ifndef TUNGOL_COMMON_INSTANCES_GLSL
#define TUNGOL_COMMON_INSTANCES_GLSL

struct Instance
{
    mat4 worldTransform;
    uint materialIndex;
    uint boneOffset;
};

layout(set = 1, binding = 0) readonly buffer Instances { Instance instances[]; };

#endif