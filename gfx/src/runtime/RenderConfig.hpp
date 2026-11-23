#pragma once
#include <vhl/core/GraphicsCommon.hpp>
#include <array>

namespace tungol
{
    namespace main
    {
        inline constexpr vhl::SampleCount k_MsaaSampleCount = vhl::SampleCount::Four;
        inline constexpr vhl::Format k_ShadowMapFormat = vhl::Format::D32Float;
        inline constexpr uint32_t k_ShadowMapResolution = 1408U;

        inline constexpr uint32_t k_MaxInstances = 2048U;
        inline constexpr uint32_t k_MaxMeshesPerInstance = 16U;

        inline constexpr uint32_t k_MaxSkinnedInstances = 256U;
        inline constexpr uint32_t k_MaxBonesPerInstance = 512U;

        inline constexpr uint32_t k_MaxTextures = 4096U;
        inline constexpr uint32_t k_MaxPlaybackCount = k_MaxSkinnedInstances;   // one playback per skinned instance max
    }

    namespace shaders
    {
        inline constexpr const char* k_ShadowStatic = "shadow/bin/static.vertex.spv";
        inline constexpr const char* k_ShadowSkinned = "shadow/bin/skinned.vertex.spv";
        inline constexpr const char* k_ShadowEmpty = "shadow/bin/empty.frag.spv";

        inline constexpr const char* k_StaticVertex = "static/bin/static.vertex.spv";
        inline constexpr const char* k_StaticFragment = "static/bin/static.frag.spv";

        inline constexpr const char* k_SkinnedVertex = "skinned/bin/skinned.vertex.spv";
        inline constexpr const char* k_SkinnedFragment = "skinned/bin/skinned.frag.spv";

        inline constexpr const char* k_BoneSample = "compute/bin/sample.comp.spv";
        inline constexpr const char* k_BoneHierarchy = "compute/bin/hierarchy.comp.spv";
    }
}
