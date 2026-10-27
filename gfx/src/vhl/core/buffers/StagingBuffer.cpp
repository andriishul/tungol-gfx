#include "StagingBuffer.hpp"

namespace tungol::vhl
{
	StagingBuffer::~StagingBuffer() = default;

	StagingBuffer::StagingBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size) 
		: Buffer(device, allocator, BufferDesc{ BufferType::Staging, MemoryType::CpuToGpu, size })
	{
	}
}
