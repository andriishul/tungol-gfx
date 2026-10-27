#ifndef TUNGOL_COMMON_TEXTURES_GLSL
#define TUNGOL_COMMON_TEXTURES_GLSL

#include "constants.glsl"
#include "materials.glsl"

layout(set = 2, binding = 0) uniform sampler2D samplers[];

#ifndef SHADOW_SET
#define SHADOW_SET 4
#endif

layout(set = SHADOW_SET, binding = 0) uniform sampler2DShadow shadowMap;

vec4 albedo_texture(const MaterialData m, vec2 uv)
{
    return (m.albedoIdx != UINT32_MAX)
        ? texture(samplers[nonuniformEXT(m.albedoIdx)], uv)
        : vec4(1.0);
}

vec4 normal_texture(const MaterialData m, vec2 uv)
{
    return (m.normalIdx != UINT32_MAX)
        ? texture(samplers[nonuniformEXT(m.normalIdx)], uv)
        : vec4(0.5, 0.5, 1.0, 1.0);
}

vec4 mr_texture(const MaterialData m, vec2 uv)
{
    return (m.mrIdx != UINT32_MAX)
        ? texture(samplers[nonuniformEXT(m.mrIdx)], uv)
        : vec4(1.0, 0.5, 0.0, 1.0);
}

vec4 emissive_texture(const MaterialData m, vec2 uv)
{
    return (m.emissiveIdx != UINT32_MAX)
        ? texture(samplers[nonuniformEXT(m.emissiveIdx)], uv)
        : vec4(0.0);
}

#endif
