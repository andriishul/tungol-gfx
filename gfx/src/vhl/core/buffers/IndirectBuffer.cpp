#include "IndirectBuffer.hpp"

namespace tungol::vhl 
{
	IndirectBuffer::~IndirectBuffer() = default;

	IndirectBuffer::IndirectBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size) 
		: Buffer(device, allocator, BufferDesc{ BufferType::Indirect, MemoryType::GpuOnly, size })
	{
	}
}