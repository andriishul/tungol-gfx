#pragma once
#include "Buffer.hpp"

namespace tungol::vhl 
{
	class TUNGOL_EXPORT VertexBuffer : public Buffer
	{
	public:
		VertexBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size);
		~VertexBuffer() override;
	};
}
