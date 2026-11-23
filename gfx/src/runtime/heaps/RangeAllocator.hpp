#pragma once
#include <cstdint>

#include <optional>
#include <algorithm>
#include <vector>

namespace tungol::runtime 
{
	struct Range 
	{
		uint64_t offset = 0;
		uint64_t size = 0;
	};

	class RangeAllocator
	{
	public:
		RangeAllocator(uint64_t size) : m_free({ Range{ 0, size } }) {}

		std::optional<Range> allocate(uint64_t size, uint64_t alignment)
		{
			for (auto it = m_free.begin(); it != m_free.end(); ++it)
			{
				auto& range = *it;

				const uint64_t rStart = range.offset;
				const uint64_t nStart = align_up(range.offset, alignment);


				const uint64_t gapSize = nStart - rStart;
				const uint64_t paddingSize = gapSize + size;

				if (range.size < paddingSize) continue;


				Range current{ nStart, size };
				Range remaining{ nStart + size, range.size - paddingSize };

				if (gapSize > 0 && remaining.size > 0)
				{
					range.size = gapSize;
					m_free.insert(it + 1, remaining);
				}
				else if (gapSize > 0)
				{
					range.size = gapSize;
				}
				else if (remaining.size > 0)
				{
					range = remaining;
				}
				else m_free.erase(it);
				 
				return current;
			}

			return std::nullopt;
		}
		void free(const Range& range) 
		{
			auto it = std::lower_bound(m_free.begin(), m_free.end(), range.offset, [](const Range& rng, uint64_t rangeOffset)
			{
				return rng.offset < rangeOffset;
			});

			it = m_free.insert(it, range);
			
			if (it != m_free.end() && it + 1 != m_free.end())
			{
				auto nextIt = it + 1;

				if (nextIt->offset == it->offset + it->size)
				{
					it->size += nextIt->size;
					m_free.erase(nextIt);
				}
			}

			if (it != m_free.begin())
			{
				auto prevIt = it - 1;

				if (it->offset == prevIt->offset + prevIt->size)
				{
					prevIt->size += it->size;
					m_free.erase(it);
				}
			}
			
		}
		
		bool canAllocate(uint64_t size, uint64_t alignment) const
		{
			for (const auto& range : m_free)
			{
				const uint64_t start = align_up(range.offset, alignment);
				if (range.size >= (start - range.offset) + size) return true;
			}
			return false;
		}
	private:
		std::vector<Range> m_free;

		uint64_t align_up(uint64_t x, uint64_t alignment) const
		{
			return (x + alignment - 1) / alignment * alignment;
		}
	};
}