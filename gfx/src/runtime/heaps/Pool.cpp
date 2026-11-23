#include "Pool.hpp"

#include <vhl/core/buffers/Buffer.hpp>

namespace tungol::runtime 
{
	Pool::Pool(std::unique_ptr<vhl::Buffer>&& buf, uint64_t size, uint64_t align)
		: m_buffer(std::move(buf)), m_capacity(size), m_alignment(align), m_rangeAlloc(size)
	{
	}
	
	vhl::Buffer& Pool::buffer() const
	{
		return *m_buffer;
	}

	Pool::~Pool()
	{
		m_buffer.reset();
		m_capacity = 0;
		m_alignment = 0;
	}

}