#pragma once
#include "Buffer.hpp"

namespace tungol::vhl 
{
	class TUNGOL_EXPORT IndexBuffer : public Buffer
	{
	public:
		IndexBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size);
		~IndexBuffer();
	};
}
