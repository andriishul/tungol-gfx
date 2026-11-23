#pragma once
#include <math/types.hpp>

namespace tungol::runtime 
{
    struct alignas(16) ShaderMaterial
    {
        math::vec4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallic = 0.0f;
        float roughness = 0.5f;
        uint32_t albedoIdx = UINT32_MAX;
        uint32_t normalIdx = UINT32_MAX;
        uint32_t mrIdx = UINT32_MAX;
        uint32_t emissiveIdx = UINT32_MAX;
    };
}