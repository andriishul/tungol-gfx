#include "UniformBuffer.hpp"

namespace tungol::vhl 
{
	UniformBuffer::~UniformBuffer() = default;

	UniformBuffer::UniformBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size) 
		: Buffer(device, allocator, BufferDesc{ BufferType::Uniform, MemoryType::GpuOnly, size })
	{
	}
}
