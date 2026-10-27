#include "IndexBuffer.hpp"

namespace tungol::vhl
{
	IndexBuffer::~IndexBuffer() = default;

	IndexBuffer::IndexBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size) 
		: Buffer(device, allocator, BufferDesc{ BufferType::Index, MemoryType::GpuOnly, size })
	{
	}
	
}
