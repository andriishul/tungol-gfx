#pragma once
#include "Pool.hpp"

namespace tungol::runtime 
{
	struct MaterialHeapSizes
	{
		uint64_t materialsSize = 0;
	};

	struct MaterialAllocation
	{
		std::optional<Range> materials;
		bool valid(uint64_t requested) const { return requested == 0 || materials.has_value(); }
	};

	class TUNGOL_EXPORT MaterialHeap 
	{
	public:
		MaterialHeap(RenderKernel& kernel, const MaterialHeapSizes& heapSizes);
		~MaterialHeap();

		MaterialAllocation allocate(uint64_t size);
		void free(const MaterialAllocation& a);

		const Pool& pool() const;

		MaterialHeapSizes sizes() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}