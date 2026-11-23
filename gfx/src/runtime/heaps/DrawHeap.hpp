#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <memory>


namespace tungol::runtime
{
	struct DrawHeapSizes
	{
		uint64_t indirectSize;
		uint64_t instancesSize;
	};

	class TUNGOL_EXPORT DrawHeap
	{
	public:
		DrawHeap(RenderKernel& kernel, const DrawHeapSizes& heapSize);
		~DrawHeap();

		vhl::Buffer& indirectBufs(uint32_t slot) const;
		vhl::Buffer& instancesSBs(uint32_t slot) const;

		vhl::Buffer& indirectTBs(uint32_t slot) const;
		vhl::Buffer& instancesTBs(uint32_t slot) const;

		const vhl::DescriptorSetHandle& objectSets(uint32_t slot) const;

		DrawHeapSizes sizes() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}