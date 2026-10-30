#include "Allocator.hpp"

#include <vhl/utils/VkInspector.hpp>

namespace tungol::vhl 
{
    constexpr uint64_t DEFAULT_SLAB_ALIGNMENT = 64 * 1024; // 64 KB

    constexpr uint32_t MAX_DEFAULT_VK_ALLOCATION_COUNT = 4096;

    constexpr uint32_t MIN_BLOCK_SIZE = 16; // Chosen because most graphics APIs prefer 16-byte alignment as a default, especially for uniform and storage buffers.
    constexpr uint32_t NUM_CLASSES = 32;


    static uint64_t alignUp(uint64_t offset, uint64_t alignment)
    {
        if (alignment == 0) throw std::runtime_error("[alignUp] alignment cannot be zero!");
        return (offset + (alignment - 1)) & ~(alignment - 1);
    }

    static uint32_t getClassIndex(uint64_t size) 
    {
        if(size == 0) throw std::runtime_error("[getClassIndex] size cannot be zero!");
        unsigned long idx = 0;
        _BitScanReverse(&idx, size);
        return std::min<uint32_t>(idx, NUM_CLASSES - 1);
    }

    struct Block
    { 
        Block* prev_free = nullptr;
        Block* next_free = nullptr;

        Block* prev = nullptr;
        Block* next = nullptr;

        uint64_t offset = 0;
        uint64_t size = 0;

        bool freed = false;
    };

    struct BlockLocation { uint32_t slabIndex = UINT32_MAX; Block* block = nullptr; };

    struct InternalSlab
    {
        InternalSlab() = default;
        InternalSlab(const InternalSlab&) = delete;
        InternalSlab(InternalSlab&& slab) noexcept :
            blockList(slab.blockList), head(slab.head),
            tail(slab.tail), memory(slab.memory),
            pMappedData(slab.pMappedData), totalSize(slab.totalSize),
            used(slab.used), memoryType(slab.memoryType),
            memoryTypeIndex(slab.memoryTypeIndex), active(slab.active)
        {
            slab.head = nullptr;
            slab.tail = nullptr;
            slab.memory = 0;
            slab.pMappedData = nullptr;
            slab.blockList.fill(nullptr);
        }

        InternalSlab& operator=(InternalSlab&& slab) noexcept
        {
            blockList = slab.blockList;
            head = slab.head;
            tail = slab.tail; 
            memory = slab.memory;
            pMappedData = slab.pMappedData; 
            totalSize = slab.totalSize;
            used = slab.used; 
            memoryType = slab.memoryType;
            memoryTypeIndex = slab.memoryTypeIndex; 
            active = slab.active;


            slab.head = nullptr;
            slab.tail = nullptr;
            slab.memory = 0;
            slab.pMappedData = nullptr;
            slab.blockList.fill(nullptr);
            return *this;
        }
        
        std::array<Block*, NUM_CLASSES> blockList{ nullptr };

        Block* head = nullptr, *tail = nullptr;

        VkDeviceMemory memory = 0;

        void* pMappedData = nullptr;

        uint64_t totalSize = 0;
        uint64_t used = 0;

        MemoryType memoryType = MemoryType::GpuOnly;
        uint32_t memoryTypeIndex = 0;
        
        bool active = false;
    };

    struct SegregatedAllocator::Impl
    {
        uint64_t bufferSlabSize = 0; 
        uint64_t imageSlabSize = 0;

        VkDevice logical = nullptr;

        std::vector<InternalSlab> bufferPool;
        std::vector<InternalSlab> imagePool;
        std::unordered_map<uint64_t, BlockLocation> bufferHandleIndex;
        std::unordered_map<uint64_t, BlockLocation> imageHandleIndex;

        uint32_t totalAllocationCounter = 0;
        
        uint32_t maxMemoryAllocationCount = MAX_DEFAULT_VK_ALLOCATION_COUNT;

        static InternalSlab createSlab(const VkDevice logical, uint64_t slabSize, MemoryType memoryType, uint32_t memoryTypeIndex, 
            uint32_t& tCounter, uint32_t mCount);
        
        static const MemoryBlock allocateFromPool(std::vector<InternalSlab>& pool, std::unordered_map<uint64_t, BlockLocation>& handleIndex,
            uint64_t rawResourceHandle, uint64_t slabSizeDefault, MemoryType memoryType, uint32_t memoryTypeIndex, uint64_t size, uint64_t alignment,
            const VkDevice logical, uint32_t& tCounter, uint32_t mCount);

        static void freeFromPool(
            std::vector<InternalSlab>& pool, std::unordered_map<uint64_t, BlockLocation>& handleIndex, uint64_t rawResourceHandle);
    };

    static void deleteBlocks(Block* head) 
    {
        while (head)
        {
            Block *ptr = head->next;
            delete head;
            head = ptr;
        }
    }
    static void shrinkPool(
        const VkDevice logical, std::vector<InternalSlab>& pool, std::unordered_map<uint64_t, BlockLocation>& handleIndex,
        uint32_t& counter, uint32_t start = 0, bool freeAll = false)
    {
        if (!pool.empty() && pool.size() > start)
        {
            uint32_t writeIndex = start;
          
            for (uint32_t readIndex = start; readIndex < static_cast<uint32_t>(pool.size()); ++readIndex)
            {
                auto& slab = pool[readIndex];

                if (freeAll || (!slab.active && !slab.used)) 
                {
                    deleteBlocks(slab.head);
                    slab.head = slab.tail = nullptr;
                    slab.blockList.fill(nullptr);

                    if ((slab.memoryType == MemoryType::CpuToGpu || slab.memoryType == MemoryType::GpuToCpu) && slab.pMappedData)
                    {
                        vkUnmapMemory(logical, slab.memory);
                    }

                    vkFreeMemory(logical, slab.memory, nullptr);
                    if(counter > 0) counter--;
                    continue;
                }

                if (writeIndex != readIndex)
                {
                    pool[writeIndex] = std::move(slab);
                    for (auto& [handle, loc] : handleIndex)
                        if (loc.slabIndex == readIndex)
                            loc.slabIndex = writeIndex;
                }

                writeIndex++;
            }

            pool.resize(writeIndex);
            
        }
    }

    // Remove a block from the free list at any position.
    static void removeBlockFromFreeList(InternalSlab& slab, Block* block)
    {
        if (!block) throw std::runtime_error("[removeBlockFromFreeList] Null block pointer");
        
        uint32_t classIndex = getClassIndex(block->size);

        Block* prevFree = block->prev_free;
        Block* nextFree = block->next_free;

        if (!prevFree) slab.blockList[classIndex] = block->next_free;
        else prevFree->next_free = nextFree;
        
        if (nextFree) nextFree->prev_free = prevFree;

        block->prev_free = nullptr;
        block->next_free = nullptr;
    }

    // Insert a block at the front of the free list.
    static void insertBlockIntoFreeList(InternalSlab& slab, Block* block)
    {
        if (!block) throw std::runtime_error("[insertBlockIntoFreeList] Null block pointer");

        uint32_t classIndex = getClassIndex(block->size);

        Block* current = slab.blockList[classIndex];

        if (current) current->prev_free = block;

        block->next_free = current;
        block->prev_free = nullptr;

        slab.blockList[classIndex] = block;
    }

    static void insertBlockAfter(InternalSlab& slab, Block* block, Block* newBlock)
    {
        if (!block || !newBlock) throw std::runtime_error("[insertBlockAfter] Null block pointer");

        if (Block* next = block->next)
        {
            next->prev = newBlock;
            newBlock->next = next;
        }

        newBlock->prev = block;
        block->next = newBlock;

        if (slab.tail == block)
            slab.tail = newBlock;
    }

    static void insertBlockBefore(InternalSlab& slab, Block* block, Block* newBlock)
    {
        if (!block || !newBlock) throw std::runtime_error("[insertBlockBefore] Null block pointer");

        if (Block* prev = block->prev) 
        {
            prev->next = newBlock;
            newBlock->prev = prev;
        }

        block->prev = newBlock;
        newBlock->next = block;

        if (slab.head == block)
            slab.head = newBlock;
    }

    static Block* findFreeBlock(InternalSlab& slab, uint64_t size, uint64_t alignment)
    {
        size += (alignment - 1);
        uint32_t classIndex = getClassIndex(size);
        
        for (uint32_t i = classIndex; i < NUM_CLASSES; ++i)
        {
            for (Block* freeBlock = slab.blockList[i]; freeBlock; freeBlock = freeBlock->next_free)
            {
                if (freeBlock->size >= size)
                    return freeBlock;
            }
        }

        return nullptr;
    }

    static Block* allocateBlock(InternalSlab& slab, uint64_t size, uint64_t alignment)
    {
        Block* freeBlock = findFreeBlock(slab, size, alignment);

        if (!freeBlock) return nullptr; // Triggering creation of new slab
        
        removeBlockFromFreeList(slab, freeBlock);

        uint64_t newOffset = alignUp(freeBlock->offset, alignment);
        uint64_t oldOffset = freeBlock->offset;

        uint64_t newEnd = newOffset + size;
        uint64_t oldEnd = oldOffset + freeBlock->size;

        if (oldOffset < newOffset) 
        {
            uint64_t gapSize = newOffset - oldOffset;

            if (gapSize >= MIN_BLOCK_SIZE)
            {
                Block* gapBlock = new Block{};
                gapBlock->freed = true;
                gapBlock->offset = oldOffset;
                gapBlock->size = gapSize;

                insertBlockBefore(slab, freeBlock, gapBlock);
                insertBlockIntoFreeList(slab, gapBlock);
            }
        }

        freeBlock->freed = false;
        freeBlock->offset = newOffset;
        freeBlock->size = size;
        
        if (oldEnd > newEnd)
        {
            uint64_t gapSize = oldEnd - newEnd;

            if (gapSize >= MIN_BLOCK_SIZE)
            {
                Block* gapBlock = new Block{};
                gapBlock->freed = true;
                gapBlock->offset = newEnd;
                gapBlock->size = gapSize;

                insertBlockAfter(slab, freeBlock, gapBlock);
                insertBlockIntoFreeList(slab, gapBlock);
            }
        }

        slab.used += size;
        
        if (slab.used > 0 && !slab.active) slab.active = true;

        return freeBlock;
    }

    static void coalesceWithPrev(InternalSlab& slab, Block* block)
    {
        Block* before = block->prev;
        
        if (before && before->freed && block->freed)
        {
            uint64_t blockEnd = block->offset + block->size;


            if (Block* prevPrev = before->prev) 
            {
                prevPrev->next = block;
            }

            block->prev = before->prev;
            block->offset = before->offset;
            block->size = blockEnd - before->offset;

            if (slab.head == before) slab.head = block;

            before->prev = nullptr;
            before->next = nullptr;

            removeBlockFromFreeList(slab, before);
            delete before;
        }

        
    }

    static void coalesceWithNext(InternalSlab& slab, Block* block)
    {
        Block* after = block->next;

        if (after && after->freed && block->freed)
        {
            uint64_t nextEnd = after->offset + after->size;

            block->next = nullptr;

            if (Block* nextNext = after->next)
            {
                block->next = nextNext;
                nextNext->prev = block;
            }

            block->size = nextEnd - block->offset;

            if (slab.tail == after) slab.tail = block;

            after->prev = nullptr;
            after->next = nullptr;

            removeBlockFromFreeList(slab, after);
            delete after;
        }
    }

    static void freeBlock(InternalSlab& slab, Block* block)
    {
        if (!block) throw std::runtime_error("[freeBlock] Null block pointer");
        if (block->freed) throw std::runtime_error("[freeBlock] Block already freed!");
        block->freed = true;
        slab.used -= block->size;
        coalesceWithPrev(slab, block);
        coalesceWithNext(slab, block);
        insertBlockIntoFreeList(slab, block);

        if (!slab.used) slab.active = false;
    }

    InternalSlab SegregatedAllocator::Impl::createSlab(const VkDevice logical, uint64_t slabSize, MemoryType memoryType, uint32_t memoryTypeIndex, uint32_t& tCounter, uint32_t mCount)
    {
        if (tCounter >= mCount)
        {
            std::ostringstream sout;
            sout << "[SegregatedAllocator::createSlab] Reached maximum limit of allocation number: "
                << tCounter << " out of " << mCount;

            throw std::runtime_error(sout.str());
        }

        InternalSlab slab{};
        slab.memoryType = memoryType;
        slab.memoryTypeIndex = memoryTypeIndex;

        VkMemoryAllocateInfo allocInfo
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = slabSize,
            .memoryTypeIndex = memoryTypeIndex
        };

        VK_CHECK(vkAllocateMemory(logical, &allocInfo, nullptr, &slab.memory));

        if (memoryType == MemoryType::CpuToGpu || memoryType == MemoryType::GpuToCpu)
        {
            try
            {
                VK_CHECK(vkMapMemory(logical, slab.memory, 0, VK_WHOLE_SIZE, 0, &slab.pMappedData));
            }
            catch (...)
            {
                if (slab.memory)
                {
                    vkFreeMemory(logical, slab.memory, nullptr);
                    slab.memory = 0;
                }
                throw;
            }
        }

        slab.totalSize = slabSize;
        slab.active = true;

        Block* initialBlock = new Block{};

        initialBlock->offset = 0;
        initialBlock->size = slabSize;
        initialBlock->freed = true;

        uint32_t classIndex = getClassIndex(slabSize);
        initialBlock->prev_free = nullptr;
        initialBlock->next_free = slab.blockList[classIndex];

        slab.head = initialBlock;
        slab.tail = initialBlock;

        if (slab.blockList[classIndex])
        {
            slab.blockList[classIndex]->prev_free = initialBlock;
        }

        slab.blockList[classIndex] = initialBlock;

        if(tCounter >= 0 && tCounter <= UINT32_MAX) tCounter += 1;

        return slab;
    }
    
    const MemoryBlock SegregatedAllocator::Impl::allocateFromPool(std::vector<InternalSlab>& pool, std::unordered_map<uint64_t, BlockLocation>& handleIndex,
        uint64_t rawResourceHandle, uint64_t slabSizeDefault, MemoryType memoryType, uint32_t memoryTypeIndex, uint64_t size, uint64_t alignment,
        const VkDevice logical, uint32_t& tCounter, uint32_t mCount)
    {
        void* pMappedData = nullptr;

        if (!rawResourceHandle) throw std::runtime_error("[SegregatedAllocator::allocate] handleKey cannot be null, because it is actual resource.");
        if (handleIndex.find(rawResourceHandle) != handleIndex.end()) throw std::runtime_error("[SegregatedAllocator::allocate] Resource is already in use.");

        for (uint32_t slabId = 0; slabId < static_cast<uint32_t>(pool.size()); ++slabId)
        {
            auto& slab = pool[slabId];
            if (!slab.active) continue;
            if (slab.memoryType != memoryType || slab.memoryTypeIndex != memoryTypeIndex) continue;

            auto newBlock = allocateBlock(slab, size, alignment);

            if (!newBlock) continue;

            handleIndex.emplace(rawResourceHandle, BlockLocation{ slabId, newBlock });

            if (memoryType == MemoryType::CpuToGpu || memoryType == MemoryType::GpuToCpu)
            {
                pMappedData = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(slab.pMappedData) + newBlock->offset);
            }

            return MemoryBlock
            {
                .memoryHandle = DeviceMemoryHandle{ reinterpret_cast<uint64_t>(slab.memory) },
                .offset = newBlock->offset,
                .size = newBlock->size,
                .pMappedData = pMappedData
            };
        }
        pMappedData = nullptr;

        for (uint32_t slabId = 0; slabId < static_cast<uint32_t>(pool.size()); ++slabId)
        {
            auto& slab = pool[slabId];

            if (slab.active) continue;
            if (slab.memoryType != memoryType || slab.memoryTypeIndex != memoryTypeIndex) continue;

            auto newBlock = allocateBlock(slab, size, alignment);

            if (!newBlock) continue;

            handleIndex.emplace(rawResourceHandle, BlockLocation{ slabId, newBlock });

            if (memoryType == MemoryType::CpuToGpu || memoryType == MemoryType::GpuToCpu)
            {
                pMappedData = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(slab.pMappedData) + newBlock->offset);
            }

            return MemoryBlock
            {
                .memoryHandle = DeviceMemoryHandle{ reinterpret_cast<uint64_t>(slab.memory) },
                .offset = newBlock->offset,
                .size = newBlock->size,
                .pMappedData = pMappedData
            };
        }

        pMappedData = nullptr;

        auto& slab = pool.emplace_back(SegregatedAllocator::Impl::createSlab(logical, slabSizeDefault, memoryType, memoryTypeIndex, tCounter, mCount));

        auto newBlock = allocateBlock(slab, size, alignment);

        if (!newBlock) throw std::runtime_error("[SegregatedAllocator::allocate] Failed to allocate block on new slab!");

        uint32_t slabId = static_cast<uint32_t>(pool.size() - 1);

        if (memoryType == MemoryType::CpuToGpu || memoryType == MemoryType::GpuToCpu)
        {
            pMappedData = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(slab.pMappedData) + newBlock->offset);
        }

        handleIndex.emplace(rawResourceHandle, BlockLocation{ slabId, newBlock });

        return MemoryBlock
        {
            .memoryHandle = DeviceMemoryHandle{ reinterpret_cast<uint64_t>(slab.memory) },
            .offset = newBlock->offset,
            .size = newBlock->size,
            .pMappedData = pMappedData
        };
    }

    void SegregatedAllocator::Impl::freeFromPool(std::vector<InternalSlab>& pool, std::unordered_map<uint64_t, BlockLocation>& handleIndex, uint64_t rawResourceHandle)
    {
        if (auto it = handleIndex.find(rawResourceHandle); it != handleIndex.end())
        {
            uint32_t slabIndex = it->second.slabIndex;

            if (slabIndex >= pool.size())
            {
                throw std::out_of_range("[SegregatedAllocator::free] Slab is out of range");
            }

            auto& slab = pool[slabIndex];
            auto block = it->second.block;

            if (!block) throw std::runtime_error("[SegregatedAllocator::free] Block is empty or lost");

            freeBlock(slab, block);

            handleIndex.erase(it);
        }
    }

    SegregatedAllocator::SegregatedAllocator(const Device& device, uint64_t bufferSlabSize, uint64_t imageSlabSize) : m_impl(new Impl{bufferSlabSize, imageSlabSize})
    {
        if (uint64_t handle_value = device.handle().value; handle_value != 0)
            m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
        else throw std::runtime_error("[SegregatedAllocator::SegregatedAllocator] Null device handle");

        m_impl->maxMemoryAllocationCount = device.limits().memory.maxMemoryAllocationCount;
    }

    SegregatedAllocator::~SegregatedAllocator()
    {
        shrinkPool(m_impl->logical, m_impl->bufferPool, m_impl->bufferHandleIndex, m_impl->totalAllocationCounter, 0, true);
        shrinkPool(m_impl->logical, m_impl->imagePool, m_impl->imageHandleIndex, m_impl->totalAllocationCounter, 0, true);
    }

    const MemoryBlock SegregatedAllocator::allocate(BufferHandle handle, MemoryType memoryType, uint32_t memoryTypeIndex, uint64_t size, uint64_t alignment)
    {
        uint64_t preferedSlabSize = std::max(m_impl->bufferSlabSize, size + alignment - 1);
        preferedSlabSize = alignUp(preferedSlabSize, DEFAULT_SLAB_ALIGNMENT);

        return Impl::allocateFromPool(m_impl->bufferPool, m_impl->bufferHandleIndex, handle.value, preferedSlabSize, memoryType, memoryTypeIndex, size, alignment, m_impl->logical, m_impl->totalAllocationCounter, m_impl->maxMemoryAllocationCount);
    }

    const MemoryBlock SegregatedAllocator::allocate(ImageHandle handle, MemoryType memoryType, uint32_t memoryTypeIndex, uint64_t size, uint64_t alignment)
    {
        uint64_t preferedSlabSize = std::max(m_impl->imageSlabSize, size + alignment - 1);
        preferedSlabSize = alignUp(preferedSlabSize, DEFAULT_SLAB_ALIGNMENT);

        return Impl::allocateFromPool(m_impl->imagePool, m_impl->imageHandleIndex, handle.value, preferedSlabSize, memoryType, memoryTypeIndex, size, alignment, m_impl->logical, m_impl->totalAllocationCounter, m_impl->maxMemoryAllocationCount);
    }

    void SegregatedAllocator::free(BufferHandle handle)
    {
        Impl::freeFromPool(m_impl->bufferPool, m_impl->bufferHandleIndex, handle.value);
    }

    void SegregatedAllocator::free(ImageHandle handle)
    {
        Impl::freeFromPool(m_impl->imagePool, m_impl->imageHandleIndex, handle.value);
    }

    // Deletes unused blocks that are not used at all, except the first one, because we will reallocate it anyway.
    void SegregatedAllocator::shrink()
    {
        shrinkPool(m_impl->logical, m_impl->bufferPool, m_impl->bufferHandleIndex, m_impl->totalAllocationCounter, 1);
        shrinkPool(m_impl->logical, m_impl->imagePool, m_impl->imageHandleIndex, m_impl->totalAllocationCounter, 1);
    }

    std::unique_ptr<SegregatedAllocator> SegregatedAllocator::create(const Device& device, uint64_t bufferSlabSize, uint64_t imageSlabSize)
    {
        auto ptr = std::unique_ptr<SegregatedAllocator>(new SegregatedAllocator(device, bufferSlabSize, imageSlabSize));

        uint64_t reservedAllocationCount = std::min<uint64_t>(MAX_DEFAULT_VK_ALLOCATION_COUNT, ptr->m_impl->maxMemoryAllocationCount);

        ptr->m_impl->bufferPool.reserve(reservedAllocationCount);
        ptr->m_impl->imagePool.reserve(reservedAllocationCount);
        return ptr;
    }



    namespace buffer 
    {
        BufferResource create(const Device& device, SegregatedAllocator& allocator, const BufferDesc& desc)
        {
            VkBuffer buffer = 0;
            VkDevice logical = nullptr;
            VkPhysicalDevice physical = nullptr;

            if (uint64_t handle_value = device.handle().value; handle_value != 0)
                logical = reinterpret_cast<VkDevice>(handle_value);
            else throw std::runtime_error("[create() Buffer] Null device handle.");
            if (uint64_t handle_value = device.adapterHandle().value; handle_value != 0)
                physical = reinterpret_cast<VkPhysicalDevice>(handle_value);
            else throw std::runtime_error("[create() Buffer()] Null adapter handle.");

            
            VkBufferCreateInfo bufferInfo
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = desc.size,
                .usage = utils::toVkBufferUsageFlags(desc.bufferType),
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VK_CHECK(vkCreateBuffer(logical, &bufferInfo, nullptr, &buffer));
           
            BufferResource resource{};
            
            try
            {
                auto reqs = Enumerator::MemoryFilter::get_buffer_requirements(logical, physical, buffer, desc.memoryType);

                resource.handle.value = reinterpret_cast<uint64_t>(buffer);

                auto memoryBlock = allocator.allocate(resource.handle, desc.memoryType, reqs.memoryTypeIndex, reqs.size, reqs.alignment);
                
                VkBindBufferMemoryInfo bindMemoryInfo
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                    .buffer = buffer,
                    .memory = reinterpret_cast<VkDeviceMemory>(memoryBlock.memoryHandle.value),
                    .memoryOffset = memoryBlock.offset
                };

                VK_CHECK(vkBindBufferMemory2(logical, 1, &bindMemoryInfo));

                resource.pMappedData = memoryBlock.pMappedData;
            }
            catch (...)
            {
                if (buffer)
                {
                    vkDestroyBuffer(logical, buffer, nullptr);
                    buffer = 0;
                }

                throw;
            }

            return resource;
        }
        void destroy(const Device& device, SegregatedAllocator& allocator, const BufferResource& resource)
        {
            VkBuffer buffer = 0;
            VkDevice logical = nullptr;

            if (uint64_t handle_value = device.handle().value; handle_value != 0)
                logical = reinterpret_cast<VkDevice>(handle_value);
            else throw std::runtime_error("[destroy() Buffer] Null device handle.");

            if (uint64_t handle_value = resource.handle.value; handle_value != 0)
                buffer = reinterpret_cast<VkBuffer>(handle_value);
            else throw std::runtime_error("[destroy() Buffer] Null buffer handle.");

            vkDestroyBuffer(logical, buffer, nullptr);
            allocator.free(resource.handle);
        }
    }
    
    namespace image 
    {
        ImageResource create(const Device& device, SegregatedAllocator& allocator, const ImageDesc& desc)
        {
            VkImage image = 0;
            VkDevice logical = nullptr;
            VkPhysicalDevice physical = nullptr;

            if (uint64_t handle_value = device.handle().value; handle_value != 0)
                logical = reinterpret_cast<VkDevice>(handle_value);
            else throw std::runtime_error("[create() Image] Null device handle.");

            if (uint64_t handle_value = device.adapterHandle().value; handle_value != 0)
                physical = reinterpret_cast<VkPhysicalDevice>(handle_value);
            else throw std::runtime_error("[create() Image] Null adapter handle.");

            VkExtent3D extent
            {
                .width = desc.width,
                .height = desc.height,
                .depth = desc.depth
            };

            VkImageCreateInfo imageInfo
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = utils::toVkImageType(desc.imageType),
                .format = utils::toVkFormat(desc.format),
                .extent = extent,
                .mipLevels = desc.levelCount,
                .arrayLayers = desc.layerCount,
                .samples = utils::toVkSampleCountFlagBits(desc.samples),
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = utils::toVkImageUsageFlags(desc.usage),
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };


            VK_CHECK(vkCreateImage(logical, &imageInfo, nullptr, &image));

            ImageResource resource{};
            try
            {
                auto reqs = Enumerator::MemoryFilter::get_image_requirements(logical, physical, image, desc.memoryType);

                resource.handle.value = reinterpret_cast<uint64_t>(image);

                auto memoryBlock = allocator.allocate(resource.handle, reqs.memoryType, reqs.memoryTypeIndex, reqs.size, reqs.alignment);

                VkBindImageMemoryInfo bindMemoryInfo
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                    .image = image,
                    .memory = reinterpret_cast<VkDeviceMemory>(memoryBlock.memoryHandle.value),
                    .memoryOffset = memoryBlock.offset
                };

                VK_CHECK(vkBindImageMemory2(logical, 1, &bindMemoryInfo));
                
                resource.pMappedData = memoryBlock.pMappedData;
            }
            catch (...)
            {
                if (image)
                {
                    vkDestroyImage(logical, image, nullptr);
                    image = 0;
                }

                throw;
            }

            // Images are OPTIMAL only in my renderer, as it is mostly GPU_ONLY resource type, than we donot need pMappedData at all.
            return resource;
        }
        void destroy(const Device& device, SegregatedAllocator& allocator, const ImageResource& resource)
        {
            VkImage image = 0;
            VkDevice logical = nullptr;

            if (uint64_t handle_value = device.handle().value; handle_value != 0)
                logical = reinterpret_cast<VkDevice>(handle_value);
            else throw std::runtime_error("[destroy() Image] Null device handle.");

            if (uint64_t handle_value = resource.handle.value; handle_value != 0)
                image = reinterpret_cast<VkImage>(handle_value);
            else throw std::runtime_error("[destroy() Image] Null image handle.");

            vkDestroyImage(logical, image, nullptr);
         
            allocator.free(resource.handle);
        }
    }
}
