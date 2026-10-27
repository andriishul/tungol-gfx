#ifndef TUNGOL_COMMON_MATERIALS_GLSL
#define TUNGOL_COMMON_MATERIALS_GLSL

struct MaterialData
{
    vec4 baseColor;
    float metallic;
    float roughness;
    uint albedoIdx;
    uint normalIdx;
    uint mrIdx;
    uint emissiveIdx;
};

layout(std430, set = 1, binding = 1) readonly buffer Materials { MaterialData materials[]; };

#endif

