#include "PoolBudget.hpp"

namespace tungol::runtime 
{
	const std::vector<PoolBudget> createDefaultPoolBudgets()
	{
		const uint64_t MB = 1024 * 1024;

		const std::vector<PoolBudget> budgets
		{
			{ "tiny",   0,               32 * MB,  12 * MB,  16 * MB,   8 * MB },
			{ "low",    1843 * MB,       96 * MB,  32 * MB,  48 * MB,  16 * MB },
			{ "medium", 3584 * MB,      192 * MB,  64 * MB,  96 * MB,  32 * MB },
			{ "high",   5632 * MB,      512 * MB, 128 * MB, 240 * MB,  64 * MB },
			{ "ultra",  9728 * MB,      768 * MB, 256 * MB, 376 * MB, 128 * MB }
		};

		return budgets;
	}

	static uint64_t fixedBytes(const PoolBudget& pB)
	{
		return pB.staticVBSize + pB.skinnedVBSize + pB.indexBufSize + pB.keyframesSize
			+ pB.keyframesSize / 4 * 3 + pB.keyframesSize / 8 + pB.keyframesSize / 16;
	}

	const PoolBudget pickBudget(const std::vector<PoolBudget>& budgets, uint64_t heapSize)
	{
		size_t i = 0;
		for (size_t k = 0; k < budgets.size(); ++k)
		{
			if (heapSize >= budgets[k].minHeap) i = k;
		}
		
		while (i > 0 && fixedBytes(budgets[i]) > heapSize * 3 / 10) --i;
		
		return budgets[i];
	}
}