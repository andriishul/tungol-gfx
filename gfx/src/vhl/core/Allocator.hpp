#pragma once
#include "Device.hpp"

namespace tungol::vhl 
{
    constexpr uint64_t BUFFER_SLAB_SIZE = 128 * 1024 * 1024; // 128 MB
    constexpr uint64_t IMAGE_SLAB_SIZE = 256 * 1024 * 1024; // 256 MB


    struct MemoryBlock
    {
        DeviceMemoryHandle memoryHandle{};

        uint64_t offset = 0;
        uint64_t size = 0;

        void* pMappedData = nullptr;
    };


    template<typename THandle>
    struct MappedResource
    {
        THandle handle{};
        void* pMappedData = nullptr;
    };


    class TUNGOL_EXPORT SegregatedAllocator
    {
    private:
        SegregatedAllocator(const Device& device, uint64_t bufferSlabSize, uint64_t imageSlabSize);
    public:
        SegregatedAllocator() = delete;
        ~SegregatedAllocator();

        const MemoryBlock allocate(BufferHandle handle, MemoryType memoryType, uint32_t memoryTypeIndex, uint64_t size, uint64_t alignment);
        const MemoryBlock allocate(ImageHandle handle, MemoryType memoryType, uint32_t memoryTypeIndex, uint64_t size, uint64_t alignment);

        void free(BufferHandle handle);
        void free(ImageHandle handle);

        void shrink();

        static std::unique_ptr<SegregatedAllocator> create(const Device& device, uint64_t bufferSlabSize = BUFFER_SLAB_SIZE, uint64_t imageSlabSize = IMAGE_SLAB_SIZE);
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
    
    struct BufferDesc 
    {
        BufferType bufferType;
        MemoryType memoryType;

        uint64_t size;
    };

    struct ImageDesc 
    {
        Format format;

        uint32_t width;
        uint32_t height;

        uint32_t depth = 1;
        
        ImageType imageType = ImageType::TwoD;
        MemoryType memoryType = MemoryType::GpuOnly;

        ImageUsageFlags usage = ImageUsageFlags::Color;

        SampleCount samples = SampleCount::One;
        uint32_t levelCount = 1;
        uint32_t layerCount = 1;
    };

    namespace image 
    {
        using ImageResource = MappedResource<ImageHandle>;

        TUNGOL_EXPORT ImageResource create(const Device&, SegregatedAllocator&, const ImageDesc&);
        TUNGOL_EXPORT void destroy(const Device&, SegregatedAllocator&, const ImageResource&);
    }

    namespace buffer
    {
        using BufferResource = MappedResource<BufferHandle>;

        TUNGOL_EXPORT BufferResource create(const Device&, SegregatedAllocator&, const BufferDesc&);
        TUNGOL_EXPORT void destroy(const Device&, SegregatedAllocator&, const BufferResource&);
    }
}
