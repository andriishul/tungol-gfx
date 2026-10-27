#pragma once
#include <cstdint>
#include <memory>

namespace tungol
{
    namespace vhl
    {
        struct AcquireResult;
        enum class Format : uint32_t;
        enum class PipelineType : uint32_t;

        class Device;
        class GraphicsContext;
        
        class Pipeline;
        struct PipelineLayoutHandle;

        class SegregatedAllocator;

        class DescriptorSetLayoutList;
        class DescriptorPool;

        struct DescriptorSetHandle;
        struct DescriptorSetLayoutHandle;

        class Buffer;
        class Image;
        class ImageView;
        class Sampler;

        class HazardTracker;
        class CommandQueue;
        class CopyContext;

        class Swapchain;
        struct SwapchainHandle;
        
        class Semaphore;
        class Fence;

        class CommandBuffer;
        class Queue;
    }

    namespace core
    {
        class ModelRegistry;
    }

    namespace runtime
    {
        struct RenderKernel;
    }

}
