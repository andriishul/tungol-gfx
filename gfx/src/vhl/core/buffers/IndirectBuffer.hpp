#pragma once
#include "Buffer.hpp"

namespace tungol::vhl 
{
	class TUNGOL_EXPORT IndirectBuffer : public Buffer
	{
	public:
		IndirectBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size);
		~IndirectBuffer() override;
	};
}
