#pragma once
#include "Buffer.hpp"

namespace tungol::vhl 
{
	class TUNGOL_EXPORT StorageBuffer : public Buffer
	{
	public:
		StorageBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size);
		~StorageBuffer() override;
	};
}
