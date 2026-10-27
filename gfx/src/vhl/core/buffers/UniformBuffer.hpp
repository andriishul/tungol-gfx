#pragma once
#include "Buffer.hpp"

namespace tungol::vhl 
{
	class TUNGOL_EXPORT UniformBuffer : public Buffer
	{
	public:
		UniformBuffer(const Device& device, SegregatedAllocator& allocator, uint64_t size);
		~UniformBuffer() override;
	};
}
