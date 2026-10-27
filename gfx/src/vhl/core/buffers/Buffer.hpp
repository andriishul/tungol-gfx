#pragma once
#include <vhl/core/Allocator.hpp>

#include <vhl/shared/Trackable.hpp>

namespace tungol::vhl 
{
	class TUNGOL_EXPORT Buffer : public Trackable
	{
	public:
		Buffer(const Device& device, SegregatedAllocator& allocator, const BufferDesc& desc);
		virtual ~Buffer();

		void* pMappedData() const;

		uint64_t size() const;

		BufferType type() const;

		BufferHandle handle() const;
		BufferLimits limits() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
