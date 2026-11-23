#pragma once
#include <cstdint>
#include <memory>

namespace tungol
{
    namespace vhl
    {
        struct AcquireResult;
        struct DrawIndirectCommand;
        struct DrawIndexedIndirectCommand;

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
        //class ModelRegistry;
        class AssetCache;
    }

    namespace runtime
    {
        struct RenderKernel;
        class ResidencyManager;

        class ShadowMap;
        class TextureList;
        
        class DrawHeap;
        class MaterialHeap;
        class GeometryHeap;
        class AnimationHeap;
    }

}
