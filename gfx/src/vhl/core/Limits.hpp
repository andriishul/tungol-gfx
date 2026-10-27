#pragma once
#include "GraphicsCommon.hpp"

#include <array>

namespace tungol::vhl 
{
    struct CommandLimits
    {
        uint32_t maxDrawIndirectCount;
        uint32_t maxPushConstantsSize;
        uint32_t maxBoundDescriptorSets;
    };

    struct DescriptorLimits
    {
        uint32_t maxPerStageUpdateAfterBindUniformBuffers;
        uint32_t maxDescriptorSetUpdateAfterBindUniformBuffers;

        uint32_t maxPerStageUpdateAfterBindStorageBuffers;
        uint32_t maxDescriptorSetUpdateAfterBindStorageBuffers;

        uint32_t maxPerStageUpdateAfterBindStorageImages;
        uint32_t maxDescriptorSetUpdateAfterBindStorageImages;

        uint32_t maxPerStageUpdateAfterBindSampledImages;
        uint32_t maxDescriptorSetUpdateAfterBindSampledImages;
    };

    struct MemoryLimits
    {
        uint32_t maxMemoryAllocationCount;
    };

    struct BufferLimits
    {
        uint64_t minUniformBufferOffsetAlignment;
        uint64_t minStorageBufferOffsetAlignment;
        uint32_t maxUniformBufferRange;
        uint32_t maxStorageBufferRange;
    };

    struct ImageLimits
    {
        uint32_t maxImageDimension2D;
        uint32_t maxImageDimensionCube;
        float maxSamplerAnisotropy;
    };

    struct ComputeLimits
    {
        std::array<uint32_t, 3> maxComputeWorkGroupCount;
        std::array<uint32_t, 3> maxComputeWorkGroupSize;
        uint32_t maxComputeWorkGroupInvocations;
        uint32_t maxComputeSharedMemorySize;
    };

    struct FramebufferLimits 
    {
        SampleCount colorSampleCounts;
        SampleCount depthSampleCounts;
        SampleCount stencilSampleCounts;
    };

    struct DeviceLimits
    {
        MemoryLimits memory;
        CommandLimits command;
        BufferLimits buffer;
        ImageLimits image;
        ComputeLimits compute;
        DescriptorLimits descriptor;
        FramebufferLimits framebuffer;
    };
}
