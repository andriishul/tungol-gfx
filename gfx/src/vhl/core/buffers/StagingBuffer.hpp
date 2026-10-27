#pragma once 
#include "Buffer.hpp"

namespace tungol::vhl 
{
	class TUNGOL_EXPORT StagingBuffer : public Buffer
	{
	public:
		StagingBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size);
		~StagingBuffer() override;
	};
}
