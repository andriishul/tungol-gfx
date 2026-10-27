#include "StorageBuffer.hpp"

namespace tungol::vhl {

	StorageBuffer::~StorageBuffer() = default;

	StorageBuffer::StorageBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size) 
		: Buffer(device, allocator, BufferDesc{ BufferType::Storage, MemoryType::GpuOnly, size })
	{
	}
}
