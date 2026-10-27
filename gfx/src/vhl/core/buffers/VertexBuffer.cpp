#include "VertexBuffer.hpp"

namespace tungol::vhl 
{
	VertexBuffer::~VertexBuffer() = default;

    VertexBuffer::VertexBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size) 
		: Buffer(device, allocator, BufferDesc{ BufferType::Vertex, MemoryType::GpuOnly, size })
	{
	}
}
