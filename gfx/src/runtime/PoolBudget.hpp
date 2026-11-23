#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <string>
#include <vector>

namespace tungol::runtime 
{
	struct PoolBudget
	{
		std::string name;
		uint64_t minHeap;

		uint64_t staticVBSize;
		uint64_t skinnedVBSize;
		uint64_t indexBufSize;

		uint64_t keyframesSize;
	};

	TUNGOL_EXPORT const std::vector<PoolBudget> createDefaultPoolBudgets();

	// Returns a PoolBudget in which fixed pools use at most 30% of the heap.
	TUNGOL_EXPORT const PoolBudget pickBudget(const std::vector<PoolBudget>& budgets, uint64_t heapSize);
}