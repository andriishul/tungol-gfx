#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include "RangeAllocator.hpp"
#include <memory>

namespace tungol::runtime 
{
	class TUNGOL_EXPORT Pool
	{
	public:
		Pool(std::unique_ptr<vhl::Buffer>&& buf, uint64_t size, uint64_t align);
		~Pool();

		std::optional<Range> allocate(uint64_t size) { return m_rangeAlloc.allocate(size, m_alignment); }
		void free(const Range& range) { m_rangeAlloc.free(range); }
		bool canAllocate(uint64_t size) const { return m_rangeAlloc.canAllocate(size, m_alignment); }

		uint64_t capacity() const { return m_capacity; }
		uint64_t alignment() const { return m_alignment; }
		
		vhl::Buffer& buffer() const;
	private:
		std::unique_ptr<vhl::Buffer> m_buffer;
		uint64_t m_capacity = 1;
		uint64_t m_alignment = 1;
		RangeAllocator m_rangeAlloc;
	};
}