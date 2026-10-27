#pragma once
#include <vhl/graph/CommandQueue.hpp>
#include <vhl/graph/HazardTracker.hpp>

namespace tungol::vhl
{
	class TUNGOL_EXPORT CopyContext
	{
	public:
		CopyContext(const Device& device, class SegregatedAllocator& allocator, CommandQueue& cmdQueue, HazardTracker& tracker);
		~CopyContext();

		CopyContext& upload(Buffer& buf, const void* pData,  uint64_t size, uint64_t offset = 0ULL);
		CopyContext& upload(Image& img, const void* pData, uint64_t size, uint32_t mipLevel = 0U, uint32_t arrayLayer = 0U);

		void flush();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
